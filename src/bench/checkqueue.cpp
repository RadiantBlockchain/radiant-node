// Copyright (c) 2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>
#include <checkqueue.h>
#include <logging.h>
#include <policy/policy.h>
#include <prevector.h>
#include <primitives/transaction.h>
#include <random.h>
#include <script/sigcache.h>
#include <streams.h>
#include <util/defer.h>
#include <util/system.h>
#include <validation.h>

#include <limits>
#include <utility>
#include <vector>

static constexpr int PREVECTOR_SIZE = 28;
static constexpr size_t QUEUE_BATCH_SIZE = 128; // this is the parameter normally used in validation.cpp

// This Benchmark tests the CheckQueue with a slightly realistic workload, where
// checks all contain a prevector that is indirect 50% of the time and there is
// a little bit of work done between calls to Add.
static void CCheckQueueSpeedPrevectorJob(benchmark::State &state) {
    static constexpr int MIN_CORES = 2;
    static constexpr size_t BATCHES = 101;
    static constexpr size_t BATCH_SIZE = 30;

    struct PrevectorJob {
        prevector<PREVECTOR_SIZE, uint8_t> p;
        PrevectorJob() {}
        explicit PrevectorJob(FastRandomContext &insecure_rand) {
            p.resize(insecure_rand.randrange(PREVECTOR_SIZE * 2));
        }
        bool operator()() { return true; }
        void swap(PrevectorJob &x) { p.swap(x.p); };
    };
    CCheckQueue<PrevectorJob> queue{QUEUE_BATCH_SIZE};
    queue.StartWorkerThreads(std::max(MIN_CORES, GetNumCores()) - 1);
    while (state.KeepRunning()) {
        // Make insecure_rand here so that each iteration is identical.
        FastRandomContext insecure_rand(true);
        CCheckQueueControl<PrevectorJob> control(&queue);
        std::vector<std::vector<PrevectorJob>> vBatches(BATCHES);
        for (auto &vChecks : vBatches) {
            vChecks.reserve(BATCH_SIZE);
            for (size_t x = 0; x < BATCH_SIZE; ++x) {
                vChecks.emplace_back(insecure_rand);
            }
            control.Add(vChecks);
        }
        // control waits for completion by RAII, but it is done explicitly here
        // for clarity
        control.Wait();
    }
    queue.StopWorkerThreads();
}

static void CCheckQueue_RealData32MB(bool cacheSigs, benchmark::State &state) {
    // This block happens to have txs where there are 166944 txins, and of those 157783 spend from the
    // same block, so it is a good candidate for this benchmark since we *have* the prevout coins already
    // and can do real sigchecks, without a need for a UTXO set for this block.
    static constexpr size_t nSameBlockCoinsExpected = 157783;
    const CBlock block = []{
        CBlock ret;
        VectorReader(SER_NETWORK, PROTOCOL_VERSION, benchmark::data::block556034 /* 32MB block */, 0) >> ret;
        return ret;
    }();

    // Step 1: Setup everything -- read all the block tx's and create a CScriptCheck "work" unit for each
    //         of the inputs for which we have the coin (there should be 157783 of these for this block).
    InitSignatureCache(); // just to be consistent -- ensure signature cache is empty
    size_t nSameBlockCoins = 0;
    std::map<TxId, CTransactionRef> txidMap;
    std::vector<std::vector<CScriptCheck>> vChecksPerTx;
    struct UnlimitedSigChecks : TxSigCheckLimiter {
        UnlimitedSigChecks() { remaining = std::numeric_limits<int64_t>::max(); }
        int64_t used() const { return std::numeric_limits<int64_t>::max() - remaining.load(); }
    };
    UnlimitedSigChecks highLimitPerTx, highLimitPerBlock;
    for (const auto & tx : block.vtx) {
        // build map of txid -> tx for below loop that "finds coins"
        txidMap.emplace(tx->GetId(), tx);
    }
    for (const auto & tx : block.vtx) {
        if (tx->IsCoinBase()) continue;
        size_t nIn = 0;
        const PrecomputedTransactionData txdata(*tx);
        std::vector<CScriptCheck> vChecksThisTx;
        for (const auto & txin : tx->vin) {
            // coin exists in this block, use it
            if (auto it = txidMap.find(txin.prevout.GetTxId()); it != txidMap.end()) {
                ++nSameBlockCoins;
                const CTxOut &prevTxOut = it->second->vout[txin.prevout.GetN()];
                const CScript &scriptPubKey = prevTxOut.scriptPubKey;
                const Amount &amount = prevTxOut.nValue;
                const ScriptExecutionContext context(nIn, scriptPubKey, amount, *tx);
                // Verify signature
                vChecksThisTx.emplace_back(context, MANDATORY_SCRIPT_VERIFY_FLAGS,
                                           cacheSigs /* whether to store results in cache */,
                                           txdata, &highLimitPerTx, &highLimitPerBlock);
            }
            ++nIn;
        }
        // we simulate the way validtion does it -- by grouping the checks on a per-tx basis
        // note that in the code in validation.cpp pushes empty vectors as "work" quite often,
        // so we don't check here that vChecksThisTx is not empty.
        vChecksPerTx.push_back(std::move(vChecksThisTx));
    }
    assert(nSameBlockCoins == nSameBlockCoinsExpected);

    // Step 2: In order to focus the benchmark on the actual CCheckQueue implementation speed,
    //         we pre-copy all the sigchecks we will do for this entire run into the below
    //         "PerIterContext", one of these per iter
    const auto nBenchIters = state.m_num_iters * state.m_num_evals + 1;
    struct PerIterContext {
        std::vector<std::vector<CScriptCheck>> vChecksPerTxCopy;
    };
    std::vector<PerIterContext> iterContext(nBenchIters, PerIterContext{vChecksPerTx});

    // Step 3: Setup threads for our CCheckQueue
    CCheckQueue<CScriptCheck> queue{QUEUE_BATCH_SIZE};
    int nThreads = gArgs.GetArg("-par", DEFAULT_SCRIPTCHECK_THREADS);
    const int nCores = std::max(GetNumCores(), 1);
    if (!nThreads) nThreads = nCores;
    else if (nThreads < 0) nThreads = std::max(1, nCores + nThreads); // negative means leave n cores free
    LogPrintf("%s: Using %d thread%s for signature verification\n", __func__, nThreads, nThreads != 1 ? "s" : "");
    --nThreads; // account for the fact that this main thread also does processing in .Wait() below
    queue.StartWorkerThreads(nThreads);
    Defer d([&queue]{
        queue.StopWorkerThreads();
    });

    // And finally: Run the benchmark
    size_t iterNum = 0, lastSigChecks = 0;
    while (state.KeepRunning()) {
        assert(iterNum < iterContext.size());
        auto & vChecksPerTxCopy = iterContext[iterNum++].vChecksPerTxCopy;
        CCheckQueueControl<CScriptCheck> control(&queue);
        // we emulate how the code in validation.cpp calls Add() on a per-tx basis, sometimes passing in an
        // empty vector
        for (auto & vChecks: vChecksPerTxCopy)
            control.Add(vChecks);
        const bool result = control.Wait();
        // we expect the sigchecker to succeed for the entire job, otherwise it aborts early if it doesn't, and this
        // benchmark would not be accurate in that case
        assert(result);
        const auto checksSoFar = highLimitPerBlock.used();
        const auto checksThisRun = checksSoFar - lastSigChecks;
        assert(checksThisRun == nSameBlockCoinsExpected); // sanity check
        lastSigChecks = checksSoFar;
    }
}

static void CCheckQueue_RealBlock_32MB_NoCacheStore(benchmark::State &state) {
    // Run the test without storing the results in the sigcache for each sigcheck; this emulates how ConnectBlock is
    // called during IBD
    CCheckQueue_RealData32MB(false, state);
}
static void CCheckQueue_RealBlock_32MB_WithCacheStore(benchmark::State &state) {
    // Run the test but store the results in the sigcache for each sigcheck; this emulates how ConnectBlock is called
    // when mining, from TestBlockValidity()
    CCheckQueue_RealData32MB(true, state);
}

BENCHMARK(CCheckQueueSpeedPrevectorJob, 1400);
BENCHMARK(CCheckQueue_RealBlock_32MB_NoCacheStore, 5);
BENCHMARK(CCheckQueue_RealBlock_32MB_WithCacheStore, 5);
