// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>
#include <consensus/validation.h>
#include <dsproof/storage.h>
#include <script/interpreter.h>
#include <script/sighashtype.h>
#include <script/sign.h>
#include <streams.h>
#include <txmempool.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/time.h>
#include <validation.h>
#include <version.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

using ByteVec = std::vector<uint8_t>;

BOOST_FIXTURE_TEST_SUITE(dsproof_tests, BasicTestingSetup)

std::vector<DoubleSpendProof> makeDupeProofs(unsigned num, uint64_t fuzz = 0) {
    std::vector<DoubleSpendProof> ret;

    CDataStream stream(
        ParseHex("0100000001f1b76b251770f5d26334c41327ef54d52cba86f77f67e5fce35611d4dad729270000000"
                 "06441c70853c2bb31d8df457613cfcae7755bf1e1c558271804e2a82f86558c182cec731014ebdb70"
                 "9da6e642ed89042dbbd6faed1853ee6299393e46bb656a4c8dae4121035303d906d781995ba837f73"
                 "757e336446bbbc49e377cb95e98d86a64c6878898feffffff01bd4397964e0000001976a9140a373c"
                 "af0ab3c2b46cd05625b8d545c295b93d7a88acb4781500"),
        SER_NETWORK, PROTOCOL_VERSION);
    CMutableTransaction inTx(deserialize, stream);
    if (fuzz) {
        TxId t = inTx.vin[0].prevout.GetTxId();
        auto N = inTx.vin[0].prevout.GetN();
        uint64_t begin;
        std::memcpy(&begin, t.begin(), std::min<size_t>(sizeof(begin), t.size()));
        begin ^= fuzz;
        std::memcpy(t.begin(), &begin, std::min<size_t>(sizeof(begin), t.size()));
        inTx.vin[0].prevout = COutPoint(t, N); // save fuzzed prevout
    }
    CTransaction tx1(inTx);
    ret.reserve(num);
    for (int i = 0; i < int(num); ++i) {
        CMutableTransaction mut(tx1);
        mut.vout[0].nValue -= (i+1) * SATOSHI;
        CTransaction tx2(mut);
        BOOST_CHECK(tx1.GetHash() != tx2.GetHash());
        ret.push_back( DoubleSpendProof::create(tx1, tx2, tx1.vin.at(0).prevout) );
        auto &proof = ret.back();
        BOOST_CHECK(!proof.isEmpty());
    }
    return ret;
}

std::vector<DoubleSpendProof> makeUniqueProofs(unsigned num) {
    std::vector<DoubleSpendProof> ret;
    ret.reserve(num);
    for (unsigned i = 0; i < num; ++i) {
        auto vec1 = makeDupeProofs(1, GetRand(std::numeric_limits<uint64_t>::max()));
        BOOST_CHECK(vec1.size() == 1);
        ret.emplace_back(std::move(vec1.front()));
    }
    return ret;
}

/// Test the COutPoint index of the m_proofs data structure:
/// Expected: that multiple proofs for the same COutPoint are possible and work.
BOOST_AUTO_TEST_CASE(dsproof_indexed_set_multiple_proofs_same_outpoint) {
    DoubleSpendProofStorage storage;
    constexpr unsigned NUM = 100;
    auto proofs = makeDupeProofs(NUM);
    BOOST_CHECK(proofs.size() == NUM);
    COutPoint prevout {proofs.front().outPoint()};
    std::set<DspId> ids;
    for (auto &proof: proofs) {
        auto before = ids.size();
        ids.insert(proof.GetId());
        BOOST_CHECK(ids.size() == before +1); // ensure new unique ID.
        storage.addOrphan(proof, ids.size());
    }
    // Check that we generated unique proofs for all the conflicts
    BOOST_CHECK(ids.size() == NUM);
    auto list = storage.findOrphans(prevout);
    // Check that all proofs for the one COutPoint in question are accounted for
    BOOST_CHECK(ids.size() == list.size());
    for (auto &pair : list) {
        // Check that the returned list contains the expected items
        BOOST_CHECK(ids.count(pair.first) == 1);
    }
}

// Test that claiming orphans works, as well as re-adding and removing
BOOST_AUTO_TEST_CASE(dsproof_claim_orphans_then_remove) {
    DoubleSpendProofStorage storage;
    BOOST_CHECK(storage.numOrphans() == 0);
    constexpr unsigned NUM = 100;
    auto proofs = makeUniqueProofs(NUM);
    BOOST_CHECK(proofs.size() == NUM);
    unsigned i = 0;
    for (auto &proof: proofs)
        storage.addOrphan(proof, ++i);
    BOOST_CHECK(storage.numOrphans() == NUM);
    BOOST_CHECK(storage.size() == NUM);

    i = 0;
    for (auto &proof: proofs) {
        storage.claimOrphan(proof.GetId());
        BOOST_CHECK(storage.numOrphans() == NUM - ++i);
    }
    BOOST_CHECK(storage.size() == NUM);
    BOOST_CHECK(storage.numOrphans() == 0);

    // re-add them as orphans again, size of container won't grow, they
    // just get re-categorized
    i = 0;
    for (auto &proof: proofs)
        storage.addOrphan(proof, ++i);
    BOOST_CHECK(storage.numOrphans() == NUM);
    BOOST_CHECK(storage.size() == NUM);

    // now remove
    for (auto &proof: proofs) {
        BOOST_CHECK(storage.exists(proof.GetId()));
        storage.remove(proof.GetId());
        BOOST_CHECK(!storage.exists(proof.GetId()));
    }
    BOOST_CHECK(storage.numOrphans() == 0);
    BOOST_CHECK(storage.size() == 0);

    // now remove already-removed
    for (auto &proof: proofs)
        storage.remove(proof.GetId());

    // nothing should have changed
    BOOST_CHECK(storage.numOrphans() == 0);
    BOOST_CHECK(storage.size() == 0);
}

// Test that orphan limits are respected
BOOST_AUTO_TEST_CASE(dsproof_orphans_limit) {
    DoubleSpendProofStorage storage;
    constexpr unsigned limit = 20;
    storage.setMaxOrphans(limit);
    BOOST_CHECK(storage.numOrphans() == 0);
    constexpr unsigned NUM = 200;

    auto proofs = makeDupeProofs(NUM, GetRand(std::numeric_limits<uint64_t>::max()));
    BOOST_CHECK(proofs.size() == NUM);
    COutPoint prevout {proofs.front().outPoint()};
    for (auto &proof : proofs) {
        storage.addOrphan(proof, 1);
    }

    // there is some fuzz factor when adding orphans, they may temporarily exceed limit, but no more than 25%
    BOOST_CHECK(storage.numOrphans() <= unsigned(limit * 1.25));
    BOOST_CHECK(storage.numOrphans() >= limit);
    BOOST_CHECK(storage.size() - storage.numOrphans() == 0);

    auto list = storage.findOrphans(prevout);
    BOOST_CHECK(list.size() == storage.numOrphans());
}

// Test that the periodic cleanup function works as expected, and reaps old orphans
BOOST_AUTO_TEST_CASE(dsproof_orphan_autoclener) {
    DoubleSpendProofStorage storage;

    constexpr unsigned NUM = 200, SECS = 50, MAX_ORPHANS = NUM * 5;
    constexpr int64_t mockStart = 2'000'000, spacing = 2;

    storage.setSecondsToKeepOrphans(SECS);
    storage.setMaxOrphans(MAX_ORPHANS); // set maximum comfortably larger than we need

    auto proofs = makeUniqueProofs(NUM);

    SetMockTime(mockStart);

    int i = 0;
    using TimeMap = std::multimap<int64_t, DoubleSpendProof>;
    TimeMap map;
    for (const auto &proof : proofs) {
        // add them 2 seconds apart
        SetMockTime(mockStart + (i * spacing));
        storage.addOrphan(proof, ++i);
        map.emplace(GetTime(), proof);
    }
    BOOST_CHECK(storage.numOrphans() == NUM);
    BOOST_CHECK(map.size() == NUM);

    // this removes old orphans and only keep recent 50 secs worth
    storage.periodicCleanup();

    const size_t expected = (storage.secondsToKeepOrphans()+1) / spacing;
    BOOST_CHECK(expected != NUM);
    // test that we only kept the last 50 seconds worth of orphans (at 2 seconds apart = 25 orphans)
    BOOST_CHECK(storage.numOrphans() == expected);

    // make sure that what was deleted was what we expected -- only items that are >=50 seconds old
    // are deleted and itmes <50 seconds old were kept
    auto cutoff = GetTime() - storage.secondsToKeepOrphans();
    for (const auto &pair : map) {
        auto &time = pair.first;
        auto &proof = pair.second;
        const bool shouldExist = time > cutoff;
        BOOST_CHECK(shouldExist == storage.exists(proof.GetId()));
    }

    SetMockTime(0); // undo mocktime
}

/// Tests the correct fucntionality of the DspIdPtr class
BOOST_AUTO_TEST_CASE(dsproof_dspidptr) {
    std::vector<DspId> dspIds;
    std::set<DspId> dspIdSet;
    std::set<DspIdPtr> dspIdPtrSet;
    constexpr size_t N = 100;
    // push 100 random id's
    size_t nulls = 0, dupes = 0, nullDupes = 0;
    for (size_t i = 0; i < N; ++i) {
        const DspId dspId{InsecureRand256()};
        dspIds.push_back(dspId);
        dspIdSet.insert(dspId);
        dspIdPtrSet.emplace(dspId);
        if (i > 0 && i % 3 == 0) {
            // push an IsNull() id every 3rd member
            dspIds.emplace_back();
            dspIdSet.emplace();
            dspIdPtrSet.emplace();
            ++nulls;
        }
        if (i > 0 && i % 7 == 0) {
            // push a dupe
            dspIds.emplace_back(dspIds.back());
            dspIdSet.emplace(dspIds.back());
            dspIdPtrSet.emplace(dspIds.back());
            ++dupes;
            nullDupes += dspIds.back().IsNull();
        }
    }
    BOOST_CHECK(nulls > 0);
    BOOST_CHECK(dupes > 0);
    BOOST_CHECK(dspIdSet.size() == dspIdPtrSet.size());
    BOOST_CHECK(dspIds.size() == N + nulls + dupes);
    BOOST_CHECK(dspIds.size() > dspIdSet.size());

    for (const auto & dspId :  dspIdSet) {
        BOOST_CHECK(dspIdPtrSet.count(dspId) == 1);
    }

    // sanity check. Empty DspId should be .IsNull()
    BOOST_CHECK(DspId{}.IsNull());

    DspIdPtr prev;
    size_t nullChecks = 0, memUsage = 0, resetChecks = 0, nonNullChecks = 0, moveChecks = 0, dupesSeen = 0;
    for (const auto &dspId : dspIds) {
        DspIdPtr p{dspId};
        BOOST_CHECK(dspIdPtrSet.count(p) == 1);
        BOOST_CHECK(dspIdPtrSet.count(dspId) == 1);
        BOOST_CHECK(dspIdSet.count(dspId) == 1);
        if (dspId.IsNull()) {
            ++nullChecks;
            // ensure .IsNull() are nullptr
            BOOST_CHECK(!bool(p));
            BOOST_CHECK(!bool(p.get()));
            BOOST_CHECK(p == DspId{});
            BOOST_CHECK(p == dspId);
        } else {
            ++nonNullChecks;
            BOOST_CHECK(bool(p));
            BOOST_CHECK(bool(p.get()));
            BOOST_CHECK(p == dspId); // check convenience operator==(DspId)
            BOOST_CHECK(*p == dspId); // check direct operator== of underlying type for sanity
        }
        memUsage += p.memUsage();
        DspIdPtr pCopy(p);
        BOOST_CHECK(p == pCopy);
        BOOST_CHECK(p.get() != pCopy.get() || (!p && !pCopy));
        BOOST_CHECK(p || p.get() == nullptr);
        BOOST_CHECK(pCopy || pCopy.get() == nullptr);

        if (prev == p) {
            ++dupesSeen;
        } else {
            BOOST_CHECK(prev != p);
            BOOST_CHECK(prev != dspId);
        }
        prev = p; // test assignment
        BOOST_CHECK(prev == p);
        BOOST_CHECK(prev == dspId);
        BOOST_CHECK(prev.get() != p.get() || (p.get() == nullptr && prev.get() == nullptr));
        if (prev.get()) {
            // redundant check again of underlying DspId
            BOOST_CHECK(*prev.get() == dspId);
        }

        if (p) {
            BOOST_CHECK(p.get() != nullptr);
            p.reset();
            BOOST_CHECK(p.get() == nullptr);
            BOOST_CHECK(p == DspId{});
            ++resetChecks;
        }

        // Test default c'tor
        {
            DspIdPtr def;
            BOOST_CHECK(!def);
            BOOST_CHECK(def.get() == nullptr);
            BOOST_CHECK(def == DspId{});
        }
        // Test move c'tor and move-assign
        if (!dspId.IsNull()) {
            ++moveChecks;
            DspIdPtr tmp = dspId;
            DspIdPtr m(std::move(tmp));
            BOOST_CHECK(!tmp);
            BOOST_CHECK(m);
            BOOST_CHECK(m == dspId);
            BOOST_CHECK(tmp != dspId);

            // move-assign
            tmp = std::move(m);
            BOOST_CHECK(tmp);
            BOOST_CHECK(!m);
            BOOST_CHECK(tmp == dspId);
            BOOST_CHECK(m != dspId);
        }
    }
    BOOST_CHECK(nullChecks > 0);
    BOOST_CHECK(nonNullChecks > 0);
    BOOST_CHECK(resetChecks > 0);
    BOOST_CHECK(moveChecks > 0);
    BOOST_CHECK(nullChecks == nulls + nullDupes);
    BOOST_CHECK(dupesSeen == dupes);
    BOOST_CHECK(nullChecks < dspIds.size());
    BOOST_CHECK(dupesSeen < dspIds.size());
    BOOST_CHECK(memUsage == dspIds.size() * sizeof(DspIdPtr) + (dspIds.size() - nullChecks) * sizeof(DspId));

    // sort and uniqueify -- this is another sanity check that operator< works ok on DspIdPtr
    std::sort(dspIds.begin(), dspIds.end());
    auto last = std::unique(dspIds.begin(), dspIds.end());
    dspIds.erase(last, dspIds.end());
    BOOST_CHECK(dspIds.size() == dspIdPtrSet.size());
    size_t i = 0;
    for (const auto &dspIdPtr : dspIdPtrSet) {
        BOOST_CHECK(dspIdPtr == dspIds.at(i));
        ++i;
    }
}

static std::pair<bool, CValidationState> ToMemPool(const CMutableTransaction &tx) EXCLUSIVE_LOCKS_REQUIRED(cs_main) {
    CValidationState state;
    const bool b = AcceptToMemoryPool(GetConfig(), g_mempool, state, MakeTransactionRef(tx),
                                      nullptr /* pfMissingInputs */, true /* bypass_limits */,
                                      Amount::zero() /* nAbsurdFee */);
    return {b, std::move(state)};
}

/// Comprehensive test that adds real tx's to the mempool and double-spends them.
/// - Tests that the proofs are generated correctly when rejecting double-spends
/// - Tests orphans and claiming of orphans
BOOST_FIXTURE_TEST_CASE(dsproof_doublespend_mempool, TestChain100Setup) {
    FlatSigningProvider provider;
    provider.keys[coinbaseKey.GetPubKey().GetID()] = coinbaseKey;
    provider.pubkeys[coinbaseKey.GetPubKey().GetID()] = coinbaseKey.GetPubKey();

    const CScript scriptPubKey = GetScriptForDestination(coinbaseKey.GetPubKey().GetID());
    const size_t firstTxIdx = m_coinbase_txns.size();

    for (int i = 0; i < COINBASE_MATURITY*2 + 1; ++i) {
        const CBlock b = CreateAndProcessBlock({}, scriptPubKey);
        m_coinbase_txns.push_back(b.vtx[0]);
    }

    // Some code-paths below need locks held
    LOCK2(cs_main, g_mempool.cs);
    BOOST_CHECK(DoubleSpendProof::IsEnabled()); // default state should be enabled
    g_mempool.clear(); // ensure mempool is clean
    BOOST_CHECK_EQUAL(g_mempool.doubleSpendProofStorage()->size(), 0u);


    // Create 100 double-spend pairs of mature coinbase txn:
    std::vector<CMutableTransaction> spends;
    spends.resize(2 * COINBASE_MATURITY);
    for (size_t i = 0; i < spends.size(); ++i) {
        const auto &cbTxRef = m_coinbase_txns.at(firstTxIdx + i/2);
        spends[i].nVersion = 1;
        spends[i].vin.resize(1);
        spends[i].vin[0].prevout = COutPoint(cbTxRef->GetId(), 0);
        spends[i].vout.resize(1);
        spends[i].vout[0].nValue = int64_t(1+i) * CENT;
        spends[i].vout[0].scriptPubKey = scriptPubKey;

        // Sign:
        const auto ok = SignSignature(provider, *cbTxRef, spends[i], 0, SigHashType().withForkId());
        BOOST_CHECK(ok);
    }

    std::vector<DoubleSpendProof> proofs;

    for (size_t i = 0; i+1 < spends.size(); i += 2) {
        const auto txNum = i / 2;
        const auto &cbTxRef = m_coinbase_txns.at(firstTxIdx + txNum);
        BOOST_CHECK_EQUAL(g_mempool.size(), txNum);

        const auto &spend1 = spends[i], &spend2 = spends[i+1];
        // Add first tx to mempool
        {
            auto [ok, state] = ToMemPool(spend1);
            BOOST_CHECK(ok);
            BOOST_CHECK(state.IsValid());
        }
        // Add second tx to mempool, check that it is rejected and that the dsproof generated is what we expect
        {
            auto [ok, state] = ToMemPool(spend2);
            BOOST_CHECK(!ok);
            BOOST_CHECK(!state.IsValid());
            BOOST_CHECK_EQUAL(state.GetRejectReason(), "txn-mempool-conflict");
            BOOST_CHECK(state.HasDspId());
            auto dsproof = DoubleSpendProof::create(CTransaction{spend2}, CTransaction{spend1},
                                                    spend1.vin[0].prevout, &cbTxRef->vout[0]);
            BOOST_CHECK(!dsproof.isEmpty());
            auto val = dsproof.validate(g_mempool, {});
            BOOST_CHECK_EQUAL(val, DoubleSpendProof::Validity::Valid);
            BOOST_CHECK_EQUAL(dsproof.GetId(), state.GetDspId());
            BOOST_CHECK(!state.GetDspId().IsNull());

            // Ensure mempool entry has the proper hash as well
            auto optIter = g_mempool.GetIter(spend1.GetId());
            BOOST_CHECK(optIter);
            if (optIter) {
                const auto & entry = *(*optIter);
                BOOST_CHECK(entry.dspHash);
                BOOST_CHECK(entry.dspHash == dsproof.GetId());
            }

            proofs.emplace_back(std::move(dsproof));
        }

        BOOST_CHECK_EQUAL(g_mempool.size(), txNum + 1); // mempool should have grown by 1
    }
    g_mempool.clear();
    BOOST_CHECK_EQUAL(g_mempool.size(), 0u);
    BOOST_CHECK_EQUAL(g_mempool.doubleSpendProofStorage()->size(), 0u);


    // ---
    // NEXT, do OPRHAN check -- ensure adding orphan, then adding tx, ends up claiming the orphan
    // ---

    // Add all the proofs as orphans
    auto *storage = g_mempool.doubleSpendProofStorage();
    NodeId nid = 0;
    for (const auto & proof : proofs) {
        storage->addOrphan(proof, ++nid);
    }

    BOOST_CHECK_EQUAL(storage->numOrphans(), std::min(proofs.size(), storage->maxOrphans()));
    BOOST_CHECK(storage->numOrphans() > 0);

    // Next, add all the spends again -- these should implicitly claim the orphans
    size_t okCt = 0, nokCt = 0;
    for (const auto &spend : spends) {
        const auto nOrphans = storage->numOrphans();
        auto [ok, state] = ToMemPool(spend);
        if (!ok) {
            // not added (was dupe)
            ++nokCt;
            BOOST_CHECK_EQUAL(state.GetRejectReason(), "txn-mempool-conflict");
        } else {
            // added, but should have claimed orphan(s)
            ++okCt;
            BOOST_CHECK_EQUAL(storage->numOrphans(), nOrphans-1);
        }
    }
    BOOST_CHECK(okCt > 0);
    BOOST_CHECK(nokCt > 0);
    BOOST_CHECK_EQUAL(okCt + nokCt, spends.size());

    // ensure all orphans are gone now
    BOOST_CHECK_EQUAL(storage->numOrphans(), 0u);

    // storage should still have the proofs though for tx's that have proofs
    BOOST_CHECK_EQUAL(g_mempool.doubleSpendProofStorage()->size(), nokCt);


    // finally, clear the mempool
    g_mempool.clear();
    BOOST_CHECK_EQUAL(g_mempool.size(), 0u);
    BOOST_CHECK_EQUAL(g_mempool.doubleSpendProofStorage()->size(), 0u);
}

BOOST_AUTO_TEST_SUITE_END()
