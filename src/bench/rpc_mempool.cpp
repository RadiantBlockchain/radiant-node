// Copyright (c) 2011-2019 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <policy/policy.h>
#include <rpc/blockchain.h>
#include <txmempool.h>

#include <univalue.h>

#include <list>
#include <vector>

static void AddTx(const CTransactionRef &tx, const Amount &fee,
                  CTxMemPool &pool) EXCLUSIVE_LOCKS_REQUIRED(cs_main, pool.cs) {
    LockPoints lp;
    pool.addUnchecked(CTxMemPoolEntry(tx, fee, /* time */ 0,
                                      /* spendsCoinbase */ false,
                                      /* sigChecks */ 1, lp));
}

static void RPCMempoolVerbose(benchmark::State &state) {
    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);

    constexpr size_t nTx = 1000;

    for (size_t i = 0; i < nTx; ++i) {
        Amount const value = int64_t(i) * COIN;
        CMutableTransaction tx = CMutableTransaction();
        tx.vin.resize(1);
        tx.vin[0].scriptSig = CScript() << OP_1;
        tx.vout.resize(1);
        tx.vout[0].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
        tx.vout[0].nValue = value;
        const CTransactionRef tx_r{MakeTransactionRef(tx)};
        AddTx(tx_r, value, pool);
    }

    while (state.KeepRunning()) {
        (void)MempoolToJSON(pool, /*verbose*/ true);
    }
}

static void RPCMempoolVerbose_10k(benchmark::State &state) {
    CTxMemPool pool;
    LOCK2(cs_main, pool.cs);

    constexpr size_t nTx = 10000;
    constexpr size_t nIns = 10;
    constexpr size_t nOuts = 10;

    for (size_t i = 0; i < nTx; ++i) {
        CMutableTransaction tx = CMutableTransaction();
        tx.vin.resize(nIns);
        for (size_t j = 0; j < nIns; ++j) {
            tx.vin[j].scriptSig = CScript() << OP_1;
        }
        tx.vout.resize(nOuts);
        for (size_t j = 0; j < nOuts; ++j) {
            tx.vin[j].scriptSig = CScript() << OP_1;
            tx.vout[j].scriptPubKey = CScript() << OP_1 << OP_EQUAL;
            tx.vout[j].nValue = int64_t(i*j) * COIN;
        }
        const CTransactionRef tx_r{MakeTransactionRef(tx)};
        AddTx(tx_r, /* fee */ int64_t(i) * COIN, pool);
    }

    while (state.KeepRunning()) {
        (void)MempoolToJSON(pool, /*verbose*/ true);
    }
}

BENCHMARK(RPCMempoolVerbose, 112);
BENCHMARK(RPCMempoolVerbose_10k, 10);
