// Copyright (c) 2011-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <config.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <consensus/tx_check.h>
#include <miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <random.h>
#include <script/scriptcache.h>
#include <txmempool.h>
#include <validation.h>
#include <validationinterface.h>

#include <list>
#include <vector>

static void DuplicateInputs(benchmark::State &state) {
    const CScript SCRIPT_PUB{CScript(OP_TRUE)};

    const CChainParams &chainparams = Params();

    CBlock block{};
    CMutableTransaction coinbaseTx{};
    CMutableTransaction naughtyTx{};

    CBlockIndex *pindexPrev = ::ChainActive().Tip();
    assert(pindexPrev != nullptr);
    block.nBits =
        GetNextWorkRequired(pindexPrev, &block, chainparams.GetConsensus());
    block.nNonce = 0;
    auto nHeight = pindexPrev->nHeight + 1;

    // Make a coinbase TX
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout = COutPoint();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = SCRIPT_PUB;
    coinbaseTx.vout[0].nValue = GetBlockSubsidy(nHeight, chainparams.GetConsensus());
    coinbaseTx.vin[0].scriptSig = CScript() << ScriptInt::fromIntUnchecked(nHeight) << OP_0;

    naughtyTx.vout.resize(1);
    naughtyTx.vout[0].nValue = Amount::zero();
    naughtyTx.vout[0].scriptPubKey = SCRIPT_PUB;

    uint64_t n_inputs =
        ((MAX_TX_SIZE - CTransaction(naughtyTx).GetTotalSize()) / 41) - 100;
    for (uint64_t x = 0; x < (n_inputs - 1); ++x) {
        naughtyTx.vin.emplace_back(TxId(GetRandHash()), 0, CScript(), 0);
    }
    naughtyTx.vin.emplace_back(naughtyTx.vin.back());

    block.vtx.push_back(MakeTransactionRef(std::move(coinbaseTx)));
    block.vtx.push_back(MakeTransactionRef(std::move(naughtyTx)));

    block.hashMerkleRoot = BlockMerkleRoot(block);

    while (state.KeepRunning()) {
        CValidationState cvstate{};
        assert(!CheckBlock(block, cvstate, chainparams.GetConsensus(),
                           BlockValidationOptions(GetConfig())
                               .withCheckPoW(false)
                               .withCheckMerkleRoot(false)));
        assert(cvstate.GetRejectReason() == "bad-txns-inputs-duplicate");
    }
}

template<size_t vinSize, size_t batchSize>
static void CheckRegularTransactionBench(benchmark::State &state) {
    FastRandomContext rng(true);
    const CScript scriptPubKey{CScript(OP_TRUE)};

    auto make_txn = [&](bool bad) {
        CMutableTransaction tx;
        tx.nVersion = 1;
        tx.vin.resize(vinSize);
        tx.vout.resize(1);
        tx.vout[0].nValue = Amount::zero();
        tx.vout[0].scriptPubKey = scriptPubKey;
        for (size_t i=0; i<vinSize; i++) {
            tx.vin[i].prevout = COutPoint(TxId(rng.rand256()), 0);
        }
        if (bad) {
            assert(vinSize > 1);
            size_t i = rng.randrange(vinSize);
            size_t j = rng.randrange(vinSize-1);
            if (j >= i) j++;
            tx.vin[j] = tx.vin[i];
        }
        assert(!CTransaction(tx).IsCoinBase());
        return tx;
    };

    std::list<CTransaction> valid_txns;
    for (size_t x = 0; x < batchSize; x++) {
        valid_txns.emplace_back(make_txn(false));
    }

    std::list<CTransaction> bad_txns;
    if (vinSize > 1) {
        for (size_t x = 0; x < batchSize; x++) {
            bad_txns.emplace_back(make_txn(true));
        }
    }

    while (state.KeepRunning()) {
        for (const auto &tx : valid_txns) {
            CValidationState state1;
            assert(CheckRegularTransaction(tx, state1));
        }
        for (const auto &tx : bad_txns) {
            CValidationState state1;
            assert(!CheckRegularTransaction(tx, state1));
            assert(state1.GetRejectReason() == "bad-txns-inputs-duplicate");
        }
    }
}

BENCHMARK(DuplicateInputs, 10);

constexpr auto CheckRegularTransaction_1 = CheckRegularTransactionBench<1, 1000>;
constexpr auto CheckRegularTransaction_2 = CheckRegularTransactionBench<2, 1000>;
constexpr auto CheckRegularTransaction_3 = CheckRegularTransactionBench<3, 1000>;
constexpr auto CheckRegularTransaction_4 = CheckRegularTransactionBench<4, 1000>;
constexpr auto CheckRegularTransaction_E1 = CheckRegularTransactionBench<10, 1000>;
constexpr auto CheckRegularTransaction_E2 = CheckRegularTransactionBench<100, 100>;
constexpr auto CheckRegularTransaction_E3 = CheckRegularTransactionBench<1000, 10>;
constexpr auto CheckRegularTransaction_E4 = CheckRegularTransactionBench<10000, 1>;

BENCHMARK(CheckRegularTransaction_1, 10000);
BENCHMARK(CheckRegularTransaction_2, 2000);
BENCHMARK(CheckRegularTransaction_3, 2000);
BENCHMARK(CheckRegularTransaction_4, 2000);
BENCHMARK(CheckRegularTransaction_E1, 1000);
BENCHMARK(CheckRegularTransaction_E2, 200);
BENCHMARK(CheckRegularTransaction_E3, 100);
BENCHMARK(CheckRegularTransaction_E4, 50);
