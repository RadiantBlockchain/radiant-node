// Copyright (c) 2016-2018 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>
#include <chainparams.h>
#include <coins.h>
#include <key.h>
#if defined(HAVE_CONSENSUS_LIB)
#include <script/bitcoinconsensus.h>
#endif
#include <script/interpreter.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/script_execution_context.h>
#include <script/standard.h>
#include <streams.h>
#include <tinyformat.h>
#include <util/defer.h>
#include <version.h>

#include <stdexcept>

static void VerifyNestedIfScript(benchmark::State &state) {
    std::vector<std::vector<uint8_t>> stack;
    CScript script;
    for (int i = 0; i < 100; ++i) {
        script << OP_1 << OP_IF;
    }
    for (int i = 0; i < 1000; ++i) {
        script << OP_1;
    }
    for (int i = 0; i < 100; ++i) {
        script << OP_ENDIF;
    }
    while (state.KeepRunning()) {
        auto stack_copy = stack;
        ScriptExecutionMetrics metrics = {};
        ScriptError error;
        auto const null_context = std::nullopt;
        bool ret = EvalScript(stack_copy, script, 0, BaseSignatureChecker(), metrics, null_context, &error);
        assert(ret);
    }
}

static void VerifyBlockScripts(bool reallyCheckSigs,
                               const uint32_t flags,
                               const std::vector<uint8_t> &blockdata, const std::vector<uint8_t> &coinsdata,
                               benchmark::State &state) {
    const auto prevParams = ::Params().NetworkIDString();
    SelectParams(CBaseChainParams::MAIN);
    // clean-up after we are done
    Defer d([&prevParams] {
        SelectParams(prevParams);
    });

    CCoinsView coinsDummy;
    CCoinsViewCache coinsCache(&coinsDummy);
    {
        CDataStream stream(coinsdata, SER_NETWORK, PROTOCOL_VERSION);
        std::map<COutPoint, Coin> coins;
        stream >> coins;
        for (const auto & [outpt, coin] : coins) {
            coinsCache.AddCoin(outpt, coin, false);
        }
    }

    const CBlock block = [&] {
        CBlock tmpblock;
        CDataStream stream(blockdata, SER_NETWORK, PROTOCOL_VERSION);
        char a = '\0';
        stream.write(&a, 1); // Prevent compaction
        stream >> tmpblock;
        return tmpblock;
    }();

    // save precomputed txdata ahead of time in case we iterate more than once, and so
    // that we concentrate the benchmark itself on VerifyScript()
    std::vector<PrecomputedTransactionData> txdataVec;
    if (reallyCheckSigs) {
        txdataVec.reserve(block.vtx.size());
        for (const auto &tx : block.vtx) {
            if (tx->IsCoinBase()) continue;
            txdataVec.emplace_back(*tx);
        }
    }


    const auto &coins = coinsCache; // get a const reference to be safe
    std::vector<const Coin *> coinsVec; // coins being spent laid out in block input order
    std::vector<std::vector<ScriptExecutionContext>> contexts; // script execution contexts for each tx
    contexts.reserve(block.vtx.size());
    for (const auto &tx : block.vtx) {
        if (tx->IsCoinBase()) continue;
        contexts.push_back(ScriptExecutionContext::createForAllInputs(*tx, coinsCache));
        for (auto &inp : tx->vin) {
            auto &coin = coins.AccessCoin(inp.prevout);
            assert(!coin.IsSpent());
            coinsVec.push_back(&coin);
        }
    }

    struct FakeSignaureChecker final : BaseSignatureChecker {
        bool VerifySignature(const std::vector<uint8_t> &, const CPubKey &, const uint256 &) const override { return true; }
        bool CheckSig(const std::vector<uint8_t> &, const std::vector<uint8_t> &, const CScript &, uint32_t) const override { return true; }
        bool CheckLockTime(const CScriptNum &) const override { return true; }
        bool CheckSequence(const CScriptNum &) const override { return true; }
    };
    const FakeSignaureChecker fakeChecker;

    while (state.KeepRunning()) {
        size_t okct = 0;
        size_t txdataVecIdx = 0, coinsVecIdx = 0;
        for (const auto &tx : block.vtx) {
            if (tx->IsCoinBase()) continue;
            unsigned inputNum = 0;
            for (auto &inp : tx->vin) {
                assert(coinsVecIdx < coinsVec.size());
                auto &coin = *coinsVec[coinsVecIdx];
                bool ok{};
                const auto &context = contexts[txdataVecIdx][inputNum];
                ScriptError serror;
                if (reallyCheckSigs) {
                    assert(txdataVecIdx < txdataVec.size());
                    const auto &txdata = txdataVec[txdataVecIdx];
                    const TransactionSignatureChecker checker(tx.get(), inputNum, coin.GetTxOut().nValue, txdata);
                    ok = VerifyScript(inp.scriptSig, coin.GetTxOut().scriptPubKey, flags, checker, context, &serror);
                } else {
                    ok = VerifyScript(inp.scriptSig, coin.GetTxOut().scriptPubKey, flags, fakeChecker, context, &serror);
                }
                if (!ok) {
                    throw std::runtime_error(
                        strprintf("Not ok: %s in tx: %s error: %s (okct: %d)",
                                  inp.prevout.ToString(), tx->GetId().ToString(), ScriptErrorString(serror), okct)
                    );
                }
                ++okct;
                ++inputNum;
                ++coinsVecIdx;
            }
            ++txdataVecIdx;
        }
    }
}

static const uint32_t flags_413567 = SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_DERSIG | SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY;
static const uint32_t flags_556034 = flags_413567 | SCRIPT_VERIFY_CHECKSEQUENCEVERIFY | SCRIPT_VERIFY_STRICTENC
                                     | SCRIPT_ENABLE_SIGHASH_FORKID | SCRIPT_VERIFY_LOW_S | SCRIPT_VERIFY_NULLFAIL;

// pure bench of just the script VM (sigchecks are trivially always skipped)
static void VerifyScripts_Block413567(benchmark::State &state) {
    VerifyBlockScripts(false, flags_413567, benchmark::data::Get_block413567(), benchmark::data::Get_coins_spent_413567(), state);
}

// pure bench of just the script VM (sigchecks are trivially always skipped)
static void VerifyScripts_Block556034(benchmark::State &state) {
    VerifyBlockScripts(false, flags_556034, benchmark::data::Get_block556034(), benchmark::data::Get_coins_spent_556034(), state);
}

// bench of the script VM *with* signature checking. The cost is usually dominated by libsecp256k1 here
static void VerifyScripts_SigsChecks_Block413567(benchmark::State &state) {
    VerifyBlockScripts(true, flags_413567, benchmark::data::Get_block413567(), benchmark::data::Get_coins_spent_413567(), state);
}

// bench of the script VM *with* signature checking. The cost is usually dominated by libsecp256k1 here
// Block 556034 is a very big block (this is very very slow).
static void VerifyScripts_SigsChecks_Block556034(benchmark::State &state) {
    VerifyBlockScripts(true, flags_556034, benchmark::data::Get_block556034(), benchmark::data::Get_coins_spent_556034(), state);
}

BENCHMARK(VerifyNestedIfScript, 100);

// These benchmarks just test the script VM itself, without doing real sigchecks
BENCHMARK(VerifyScripts_Block413567, 60);
BENCHMARK(VerifyScripts_Block556034, 3);

// These benchmarks do a full end-to-end test of the VM, including sigchecks.
// Since sigchecks dominate the cost here, this is slow, and as a result
// may not reveal much about the efficiency of the script interpreter itself.
// Consequently, if concerned with optimizing the script interpreter, it may
// be better to prefer the above two benchmarcks over the below two for
// measuring the script interpreter's own efficiency.
BENCHMARK(VerifyScripts_SigsChecks_Block413567, 2);
BENCHMARK(VerifyScripts_SigsChecks_Block556034, 1);
