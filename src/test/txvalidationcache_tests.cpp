// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <config.h>
#include <consensus/validation.h>
#include <key.h>
#include <keystore.h>
#include <miner.h>
#include <policy/policy.h>
#include <pubkey.h>
#include <random.h>
#include <script/scriptcache.h>
#include <script/sighashtype.h>
#include <script/sign.h>
#include <txmempool.h>
#include <util/time.h>
#include <validation.h>

#include <test/lcg.h>
#include <test/setup_common.h>
#include <test/sigutil.h>

#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(txvalidationcache_tests)

static bool ToMemPool(const CMutableTransaction &tx) {
    LOCK(cs_main);

    CValidationState state;
    return AcceptToMemoryPool(
        GetConfig(), g_mempool, state, MakeTransactionRef(tx),
        nullptr /* pfMissingInputs */, true /* bypass_limits */,
        Amount::zero() /* nAbsurdFee */);
}

BOOST_FIXTURE_TEST_CASE(tx_mempool_block_doublespend, TestChain100Setup) {
    // Make sure skipping validation of transactions that were validated going
    // into the memory pool does not allow double-spends in blocks to pass
    // validation when they should not.
    CScript scriptPubKey = CScript() << ToByteVector(coinbaseKey.GetPubKey())
                                     << OP_CHECKSIG;

    // Create a double-spend of mature coinbase txn:
    std::vector<CMutableTransaction> spends;
    spends.resize(2);
    for (int i = 0; i < 2; i++) {
        spends[i].nVersion = 1;
        spends[i].vin.resize(1);
        spends[i].vin[0].prevout = COutPoint(m_coinbase_txns[0]->GetId(), 0);
        spends[i].vout.resize(1);
        spends[i].vout[0].nValue = 11 * CENT;
        spends[i].vout[0].scriptPubKey = scriptPubKey;

        // Sign:
        std::vector<uint8_t> vchSig;
        uint256 hash = SignatureHash(scriptPubKey, CTransaction(spends[i]), 0,
                                     SigHashType().withForkId(),
                                     m_coinbase_txns[0]->vout[0].nValue);
        BOOST_CHECK(coinbaseKey.SignECDSA(hash, vchSig));
        vchSig.push_back(uint8_t(SIGHASH_ALL | SIGHASH_FORKID));
        spends[i].vin[0].scriptSig << vchSig;
    }

    CBlock block;

    // Test 1: block with both of those transactions should be rejected.
    block = CreateAndProcessBlock(spends, scriptPubKey);
    BOOST_CHECK(::ChainActive().Tip()->GetBlockHash() != block.GetHash());

    // Test 2: ... and should be rejected if spend1 is in the memory pool
    BOOST_CHECK(ToMemPool(spends[0]));
    block = CreateAndProcessBlock(spends, scriptPubKey);
    BOOST_CHECK(::ChainActive().Tip()->GetBlockHash() != block.GetHash());
    g_mempool.clear();

    // Test 3: ... and should be rejected if spend2 is in the memory pool
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(spends, scriptPubKey);
    BOOST_CHECK(::ChainActive().Tip()->GetBlockHash() != block.GetHash());
    g_mempool.clear();

    // Final sanity test: first spend in mempool, second in block, that's OK:
    std::vector<CMutableTransaction> oneSpend;
    oneSpend.push_back(spends[0]);
    BOOST_CHECK(ToMemPool(spends[1]));
    block = CreateAndProcessBlock(oneSpend, scriptPubKey);
    BOOST_CHECK(::ChainActive().Tip()->GetBlockHash() == block.GetHash());
    // spends[1] should have been removed from the mempool when the block with
    // spends[0] is accepted:
    BOOST_CHECK_EQUAL(g_mempool.size(), 0U);
}

static inline bool
CheckInputs(const CTransaction &tx, CValidationState &state,
            const CCoinsViewCache &view, bool fScriptChecks,
            const uint32_t flags, bool sigCacheStore, bool scriptCacheStore,
            const PrecomputedTransactionData &txdata, int &nSigChecksOut,
            std::vector<CScriptCheck> *pvChecks,
            CheckInputsLimiter *pBlockLimitSigChecks = nullptr)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    // nSigChecksTxLimiter need to outlive this function call, because test
    // cases are using pvChecks, so the verification is done asynchronously.
    static TxSigCheckLimiter nSigChecksTxLimiter;
    nSigChecksTxLimiter = TxSigCheckLimiter();
    return CheckInputs(tx, state, view, fScriptChecks, flags, sigCacheStore,
                       scriptCacheStore, txdata, nSigChecksOut,
                       nSigChecksTxLimiter, pBlockLimitSigChecks, pvChecks);
}

// Run CheckInputs (using pcoinsTip) on the given transaction, for all script
// flags. Test that CheckInputs passes for all flags that don't overlap with the
// failing_flags argument, but otherwise fails.
// CHECKLOCKTIMEVERIFY and CHECKSEQUENCEVERIFY (and future NOP codes that may
// get reassigned) have an interaction with DISCOURAGE_UPGRADABLE_NOPS: if the
// script flags used contain DISCOURAGE_UPGRADABLE_NOPS but don't contain
// CHECKLOCKTIMEVERIFY (or CHECKSEQUENCEVERIFY), but the script does contain
// OP_CHECKLOCKTIMEVERIFY (or OP_CHECKSEQUENCEVERIFY), then script execution
// should fail.
// Capture this interaction with the upgraded_nop argument: set it when
// evaluating any script flag that is implemented as an upgraded NOP code.
static void
ValidateCheckInputsForAllFlags(const CTransaction &tx, uint32_t failing_flags,
                               uint32_t required_flags, bool add_to_cache,
                               int expected_sigchecks)
    EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    PrecomputedTransactionData txdata(tx);

    MMIXLinearCongruentialGenerator lcg;
    for (int i = 0; i < 4096; i++) {
        uint32_t test_flags = lcg.next() | required_flags;
        CValidationState state;

        int nSigChecksDirect = 0xf00d;
        bool ret = CheckInputs(tx, state, pcoinsTip.get(), true, test_flags,
                               true, add_to_cache, txdata, nSigChecksDirect);

        // CheckInputs should succeed iff test_flags doesn't intersect with
        // failing_flags
        bool expected_return_value = !(test_flags & failing_flags);
        BOOST_CHECK_EQUAL(ret, expected_return_value);

        if (ret) {
            BOOST_CHECK(nSigChecksDirect == expected_sigchecks);
        }

        // Test the caching
        if (ret && add_to_cache) {
            // Check that we get a cache hit if the tx was valid
            std::vector<CScriptCheck> scriptchecks;
            int nSigChecksCached = 0xbeef;
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true,
                                    test_flags, true, add_to_cache, txdata,
                                    nSigChecksCached, &scriptchecks));
            BOOST_CHECK(nSigChecksCached == nSigChecksDirect);
            BOOST_CHECK(scriptchecks.empty());
        } else {
            // Check that we get script executions to check, if the transaction
            // was invalid, or we didn't add to cache.
            std::vector<CScriptCheck> scriptchecks;
            int nSigChecksUncached = 0xbabe;
            BOOST_CHECK(CheckInputs(tx, state, pcoinsTip.get(), true,
                                    test_flags, true, add_to_cache, txdata,
                                    nSigChecksUncached, &scriptchecks));
            BOOST_CHECK(!ret || nSigChecksUncached == 0);
            BOOST_CHECK_EQUAL(scriptchecks.size(), tx.vin.size());
        }
    }
}

BOOST_AUTO_TEST_CASE(scriptcache_values) {
    LOCK(cs_main);
    // Test insertion and querying of keys&values from the script cache.

    // Define a couple of macros (handier than functions since errors will print
    // out the correct line number)
#define CHECK_CACHE_HAS(key, expected_sigchecks)                               \
    {                                                                          \
        int nSigChecksRet(0x12345678 ^ (expected_sigchecks));                  \
        BOOST_CHECK(IsKeyInScriptCache(key, false, nSigChecksRet));            \
        BOOST_CHECK(nSigChecksRet == (expected_sigchecks));                    \
    }
#define CHECK_CACHE_MISSING(key)                                               \
    {                                                                          \
        int dummy;                                                             \
        BOOST_CHECK(!IsKeyInScriptCache(key, false, dummy));                   \
    }

    InitScriptExecutionCache();

    // construct four distinct keys from very slightly different data
    CMutableTransaction tx1;
    tx1.nVersion = 1;
    CMutableTransaction tx2;
    tx2.nVersion = 2;
    uint32_t flagsA = 0x7fffffff;
    uint32_t flagsB = 0xffffffff;
    ScriptCacheKey key1A(CTransaction(tx1), flagsA);
    ScriptCacheKey key1B(CTransaction(tx1), flagsB);
    ScriptCacheKey key2A(CTransaction(tx2), flagsA);
    ScriptCacheKey key2B(CTransaction(tx2), flagsB);

    BOOST_CHECK(key1A == key1A);
    BOOST_CHECK(!(key1A == key1B));
    BOOST_CHECK(!(key1A == key2A));
    BOOST_CHECK(!(key1A == key2B));
    BOOST_CHECK(key1B == key1B);
    BOOST_CHECK(!(key1B == key2A));
    BOOST_CHECK(!(key1B == key2B));
    BOOST_CHECK(key2A == key2A);
    BOOST_CHECK(!(key2A == key2B));
    BOOST_CHECK(key2B == key2B);

    // Key is not yet inserted.
    CHECK_CACHE_MISSING(key1A);
    // Add the key and check it worked
    AddKeyInScriptCache(key1A, 42);
    CHECK_CACHE_HAS(key1A, 42);

    CHECK_CACHE_MISSING(key1B);
    CHECK_CACHE_MISSING(key2A);
    CHECK_CACHE_MISSING(key2B);

    // 0 may be stored
    AddKeyInScriptCache(key1B, 0);

    // Calculate the most possible transaction sigchecks that can occur in a
    // standard transaction, and make sure the cache can hold it.
    //
    // To be pessimistic, use consensus (MAX_TX_SIZE) instead of policy
    // (MAX_STANDARD_TX_SIZE) since that particular policy limit is bypassed on
    // testnet.
    //
    // Assume that a standardness rule limiting density to ~33 bytes/sigcheck is
    // in place.
    const int max_standard_sigchecks = 1 + (MAX_TX_SIZE / 33);
    AddKeyInScriptCache(key2A, max_standard_sigchecks);

    // Read out values again.
    CHECK_CACHE_HAS(key1A, 42);
    CHECK_CACHE_HAS(key1B, 0);
    CHECK_CACHE_HAS(key2A, max_standard_sigchecks);
    CHECK_CACHE_MISSING(key2B);

    // Try overwriting an existing entry with different value (should never
    // happen in practice but see what happens).
    AddKeyInScriptCache(key1A, 99);
    // This succeeds without error, but (currently) no replacement is done.
    // It would also be acceptable to overwrite, but if we ever come to a
    // situation where this matters then neither alternative is better.
    CHECK_CACHE_HAS(key1A, 42);
}

BOOST_AUTO_TEST_SUITE_END()
