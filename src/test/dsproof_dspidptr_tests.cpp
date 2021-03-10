// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dsproof/dspid.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <vector>

BOOST_FIXTURE_TEST_SUITE(dsproof_dspidptr_tests, BasicTestingSetup)

/// Tests the regularity of the DspIdPtr class
BOOST_AUTO_TEST_CASE(dsproof_dspidptr_regularity) {
    {
        DspIdPtr a;
        DspIdPtr b;

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a{};
        DspIdPtr b{};

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a{uint256{}};
        DspIdPtr b{uint256{}};

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a{uint256{}};
        DspIdPtr b{};

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a;
        DspIdPtr b{uint256{}};

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a;
        DspIdPtr b = a;

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a{uint256{}};
        DspIdPtr b = a;

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a{uint256{std::vector<uint8_t>{1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0}}};
        DspIdPtr b = a;

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a;
        a = uint256{std::vector<uint8_t>{1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0}};
        DspIdPtr b = a;

        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }

    {
        DspIdPtr a{uint256{std::vector<uint8_t>{1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0,1,0}}};
        DspIdPtr b;

        BOOST_CHECK(a != b);
        BOOST_CHECK(b != a);
        BOOST_CHECK(!(a == b));
        BOOST_CHECK(!(b == a));

        b = a;
        BOOST_CHECK(a == a);
        BOOST_CHECK(a == b);
        BOOST_CHECK(b == a);
        BOOST_CHECK(!(a != a));
        BOOST_CHECK(!(a != b));
        BOOST_CHECK(!(b != a));
    }
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

BOOST_AUTO_TEST_SUITE_END()
