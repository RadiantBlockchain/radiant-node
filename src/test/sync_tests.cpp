// Copyright (c) 2012-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sync.h>
#include <test/setup_common.h>
#include <util/defer.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

namespace {
template <typename MutexType>
void TestPotentialDeadLockDetected(MutexType &mutex1, MutexType &mutex2) {
    { LOCK2(mutex1, mutex2); }
    bool error_thrown = false;
    try {
        LOCK2(mutex2, mutex1);
    } catch (const std::logic_error &e) {
        BOOST_CHECK_EQUAL(e.what(), "potential deadlock detected");
#ifdef DEBUG_LOCKORDER
        auto &pe = dynamic_cast<const PotentialDeadlockError &>(e);
        BOOST_CHECK_EQUAL(pe.prevOrder.first, static_cast<void *>(&mutex1));
        BOOST_CHECK_EQUAL(pe.prevOrder.second, static_cast<void *>(&mutex2));
        BOOST_CHECK_EQUAL(pe.curOrder.first, static_cast<void *>(&mutex2));
        BOOST_CHECK_EQUAL(pe.curOrder.second, static_cast<void *>(&mutex1));
#endif
        error_thrown = true;
    }
#ifdef DEBUG_LOCKORDER
    BOOST_CHECK(error_thrown);
#else
    BOOST_CHECK(!error_thrown);
#endif
}
} // namespace

BOOST_FIXTURE_TEST_SUITE(sync_tests, BasicTestingSetupWithDeadlockExceptions)

BOOST_AUTO_TEST_CASE(potential_deadlock_detected) {
    RecursiveMutex rmutex1, rmutex2;
    TestPotentialDeadLockDetected(rmutex1, rmutex2);

    Mutex mutex1, mutex2;
    TestPotentialDeadLockDetected(mutex1, mutex2);

    SharedMutex shared1, shared2;
    TestPotentialDeadLockDetected(shared1, shared2);
}

BOOST_AUTO_TEST_CASE(shared_mutex_tests) {
    std::vector<std::thread> threads;
    using namespace std::chrono_literals;
    struct {
        SharedMutex cs;
        std::atomic_int shared_ct = 0;
        int exclusive_ct GUARDED_BY(cs) = 0;
    } s;

    constexpr size_t max_predicates = 10; // update this if adding more subthreads to this test
    std::array<bool, max_predicates> predicates;
    for (auto &pred : predicates) pred = true; // start all predicates out as "vacuously true"

    size_t thr_idx = 0;

    threads.emplace_back([&s](bool *pred){
        std::this_thread::sleep_for(40ms);
        LOCK(s.cs);
        ++s.exclusive_ct;
        Defer d([&]{
            AssertLockHeld(s.cs);
            --s.exclusive_ct;
        });
        // cannot do BOOST_CHECK in a thread, so we must save predicate check value here
        *pred = s.shared_ct.load() == 0;
    }, &predicates.at(thr_idx++));
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&s](bool *pred){
            LOCK_SHARED(s.cs);
            ++s.shared_ct;
            Defer d([&]{ --s.shared_ct; });
            // cannot do BOOST_CHECK in a thread, so we must save predicate check value here
            *pred = s.exclusive_ct == 0;
            std::this_thread::sleep_for(10ms);
        }, &predicates.at(thr_idx++));
    }
    for (auto & thr : threads) {
        thr.join();
    }

    // Additional test that multiple threads can lock shared, also
    // test that REVERSE_LOCK works on a shared lock as expected,
    // and that WAIT_LOCK_SHARED and TRY_LOCK_SHARED work as expected.
    SharedMutex sm;
    WAIT_LOCK_SHARED(sm, lock);
    BOOST_CHECK(lock.owns_lock());
    // ensure a second thread can acquire shared
    std::thread([&sm](bool *pred){
        TRY_LOCK_SHARED(sm, lock2);
        *pred = lock2.owns_lock();
        if (lock2.owns_lock()){
            // also check that reverse lock works ok
            REVERSE_LOCK(lock2);
            *pred = !lock2.owns_lock() && *pred;
        }
        *pred = lock2.owns_lock() && *pred;
    }, &predicates.at(thr_idx++)).join();

    BOOST_CHECK(thr_idx <= max_predicates);

    // Check that all predicates (which were assigned to a sub-thread) are true
    // Note again: we cannot do BOOST_CHECK in a subthread which is why we must do this.
    BOOST_CHECK(std::all_of(predicates.begin(), predicates.begin() + thr_idx,
                            [](bool pred) { return pred; }));
}

BOOST_AUTO_TEST_SUITE_END()
