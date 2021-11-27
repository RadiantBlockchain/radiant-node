// Copyright (c) 2019 Greg Griffith
// Copyright (c) 2019-2020 The Bitcoin Unlimited developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sync.h>
#include <test/setup_common.h>
#include <util/time.h>

#include <boost/test/unit_test.hpp>

#include <atomic>
#include <memory>
#include <optional>
#include <thread>

// The below code explicitly deadlocks in order to test the deadlock detector.
// This leads to false positives for thread sanitizers. So we disable this test
// if compiling with -fsanitize=thread.
#if defined(__SANITIZE_THREAD__)
#  define SKIP_SANITIZER_NOT_SUPPORTED
#elif defined(__has_feature)
#  if __has_feature(thread_sanitizer)
#    define SKIP_SANITIZER_NOT_SUPPORTED
#  endif
#endif

BOOST_FIXTURE_TEST_SUITE(deadlock_tests, BasicTestingSetupWithDeadlockExceptions)

#if defined(DEBUG_LOCKORDER) && !defined(SKIP_SANITIZER_NOT_SUPPORTED) // this ifdef covers the bulk of this file

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wthread-safety-analysis"
#endif

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
// GCC warns excessively about shadowed variable names, which we use in lambdas in
// this test for clarity. So disable that warning.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wshadow"
#endif

// shared lock a shared mutex
// then try to exclusive lock the same shared mutex while holding shared lock,
// should self deadlock
BOOST_AUTO_TEST_CASE(test1) {
    SharedMutex shared_mutex;
    LOCK_SHARED(shared_mutex);
    BOOST_CHECK_THROW(LOCK(shared_mutex), PotentialDeadlockError);
}


// RecursiveMutex version of test1 (self deadlock should not be tripped up here)
BOOST_AUTO_TEST_CASE(test1r) {
    RecursiveMutex mutex;
    LOCK(mutex);
    BOOST_CHECK_NO_THROW(LOCK(mutex));
}


// exclusive lock a shared mutex
// then try to shared lock the same shared mutex while holding the exclusive
// lock, should self deadlock
BOOST_AUTO_TEST_CASE(test2) {
    SharedMutex shared_mutex;
    LOCK(shared_mutex);
    BOOST_CHECK_THROW(LOCK_SHARED(shared_mutex), PotentialDeadlockError);
}


// shared lock a shared mutex
// then try to shared lock the same shared mutex while holding the original
// shared lock, should self deadlock, no recursion allowed in a shared mutex
BOOST_AUTO_TEST_CASE(test3) {
    SharedMutex shared_mutex;
    LOCK_SHARED(shared_mutex);
    BOOST_CHECK_THROW(LOCK_SHARED(shared_mutex), PotentialDeadlockError);
}


// exclusive lock a shared mutex
// then try to exclusive likc the same shared mutex while holding the original
// exclusive lock, should self deadlock, no recursion allowed in a shared mutex
BOOST_AUTO_TEST_CASE(test4) {
    SharedMutex shared_mutex;
    LOCK(shared_mutex);
    BOOST_CHECK_THROW(LOCK(shared_mutex), PotentialDeadlockError);
}


// 2 shared mutex lock themselves then try to
// shared lock each other
// this should deadlock and throw an exception
// We use a "global" (static) shared mutex here.
BOOST_AUTO_TEST_CASE(test5) {
    static SharedMutex mutexA;
    static SharedMutex mutexB;
    struct Context {
        std::atomic<bool> done{false};
        std::atomic<int> lock_exceptions{0};
        std::atomic<int> writelocks{0};
    };
    using SharedCtx = std::shared_ptr<Context>;
    SharedCtx ctx = std::make_shared<Context>();

    auto TestThread1 = [](SharedCtx ctx){
        LOCK(mutexA);
        ++ctx->writelocks;
        while (ctx->writelocks != 2) ;
        try {
            LOCK_SHARED(mutexB);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto TestThread2 = [](SharedCtx ctx){
        LOCK(mutexB);
        ++ctx->writelocks;
        while (ctx->writelocks != 2) ;
        try {
            LOCK_SHARED(mutexA);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    std::thread thread1(TestThread1, ctx);
    std::thread thread2(TestThread2, ctx);
    Tic elapsed;
    while (!ctx->lock_exceptions && elapsed.secs() < 5) ; // wait for predicate or 5 seconds, whichever is sooner
    ctx->done = true;
    if (ctx->lock_exceptions != 1) {
        // test failure -- detach threads in this case so process can proceed without hanging
        thread1.detach();
        thread2.detach();
    } else {
        thread1.join();
        thread2.join();
    }
    BOOST_CHECK(ctx->lock_exceptions == 1);
}


// two shared mutex (A, B)
// thread1 lock_shared A,
// thread2 lock B
// thread1 lock_shared B
// thread2 lock A, should deadlock here
// because thread1 is holding a shared lock on A and is waiting for B
// while thread2 is holding an exclusive lock on B and is waiting for A
BOOST_AUTO_TEST_CASE(test6) {
    struct Context {
        SharedMutex mutexA;
        SharedMutex mutexB;
        std::atomic<bool> done{false};
        std::atomic<int> lock_exceptions{0};
        std::atomic<int> writelocks{0};
        std::atomic<int> readlocks{0};
    };
    using SharedCtx = std::shared_ptr<Context>;
    SharedCtx ctx = std::make_shared<Context>();

    auto Thread1 = [](SharedCtx ctx){
        LOCK_SHARED(ctx->mutexA);
        ++ctx->readlocks;
        while (ctx->writelocks != 1) ;
        try {
            LOCK_SHARED(ctx->mutexB);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto Thread2 = [](SharedCtx ctx){
        while (ctx->readlocks != 1) ;
        LOCK(ctx->mutexB);
        ++ctx->writelocks;
        try {
            LOCK(ctx->mutexA);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    std::thread thread1(Thread1, ctx);
    std::thread thread2(Thread2, ctx);
    Tic elapsed;
    while (!ctx->lock_exceptions && elapsed.secs() < 5) ; // wait for predicate or 5 seconds, whichever is sooner
    ctx->done = true;
    if (ctx->lock_exceptions != 1) {
        // test failure -- detach threads in this case so process can proceed without hanging
        thread1.detach();
        thread2.detach();
    } else {
        thread1.join();
        thread2.join();
    }
    BOOST_CHECK(ctx->lock_exceptions == 1);
}


// Threads 1 2 3 and shared mutex A B C
// Thread1 lock_shared A
// Thread2 lock_shared B
// Thread3 lock_shared C
// Thread1 lock B
// Thread2 lock C
// Thread3 lock A
// This tests locking race conditions as well as 3 way and higher lock ordering issues,
// the test is not specific on which thread will deadlock when trying to exclusively lock
// the above indicated shared mutex but one of them will.
BOOST_AUTO_TEST_CASE(test7) {
    struct Context {
        SharedMutex mutexA;
        SharedMutex mutexB;
        SharedMutex mutexC;

        std::atomic<bool> done{false};
        std::atomic<int> lock_exceptions{0};
        std::atomic<int> readlocks{0};
    };
    using SharedCtx = std::shared_ptr<Context>;
    SharedCtx ctx = std::make_shared<Context>();

    auto Thread1 = [](SharedCtx ctx){
        LOCK_SHARED(ctx->mutexA); // 1
        ++ctx->readlocks;
        while (ctx->readlocks != 3) ;
        try {
            LOCK(ctx->mutexB);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto Thread2 = [](SharedCtx ctx) {
        while (ctx->readlocks != 1) ;
        LOCK_SHARED(ctx->mutexB); // 2
        ++ctx->readlocks;
        while (ctx->readlocks != 3) ;
        try {
            LOCK(ctx->mutexC);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto Thread3 = [](SharedCtx ctx){
        while (ctx->readlocks != 2) ;
        LOCK_SHARED(ctx->mutexC); // 3
        ++ctx->readlocks;
        while (ctx->readlocks != 3) ;
        try {
            LOCK(ctx->mutexA);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    std::thread thread1(Thread1, ctx);
    std::thread thread2(Thread2, ctx);
    std::thread thread3(Thread3, ctx);
    Tic elapsed;
    while (!ctx->lock_exceptions && elapsed.secs() < 5) ;
    ctx->done = true;
    if (ctx->lock_exceptions != 1) {
        // test failure -- detach threads in this case so process can proceed without hanging
        thread1.detach();
        thread2.detach();
        thread3.detach();
    } else {
        thread1.join();
        thread2.join();
        thread3.join();
    }
    BOOST_CHECK(ctx->lock_exceptions == 1);
}


// Threads 1 2 3 and shared mutex A B C
// Thread1 lock A
// Thread2 lock B
// Thread3 lock C
// Thread1 lock_shared B
// Thread2 lock_shared C
// Thread3 lock_shared A
// This tests locking race conditions as well as 3 way and higher lock ordering issues,
// the test is not specific on which thread will deadlock when trying to shared lock
// the above indicated shared mutex but one of them will.
BOOST_AUTO_TEST_CASE(test8) {
    struct Context {
        SharedMutex mutexA;
        SharedMutex mutexB;
        SharedMutex mutexC;

        std::atomic<bool> done{false};
        std::atomic<int> lock_exceptions{0};
        std::atomic<int> writelocks{0};
    };
    using SharedCtx = std::shared_ptr<Context>;
    SharedCtx ctx = std::make_shared<Context>();

    auto Thread1 = [](SharedCtx ctx){
        LOCK(ctx->mutexA); // 1
        ++ctx->writelocks;
        while (ctx->writelocks != 3) ;
        try {
            LOCK_SHARED(ctx->mutexB);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto Thread2 = [](SharedCtx ctx){
        while (ctx->writelocks != 1) ;
        LOCK(ctx->mutexB); // 2
        ++ctx->writelocks;
        while (ctx->writelocks != 3) ;
        try {
            LOCK_SHARED(ctx->mutexC);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto Thread3 = [](SharedCtx ctx){
        while (ctx->writelocks != 2) ;
        LOCK(ctx->mutexC);
        ++ctx->writelocks;
        while (ctx->writelocks != 3) ;
        try {
            LOCK_SHARED(ctx->mutexA);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    std::thread thread1(Thread1, ctx);
    std::thread thread2(Thread2, ctx);
    std::thread thread3(Thread3, ctx);
    while (!ctx->lock_exceptions) ;
    ctx->done = true;
    if (ctx->lock_exceptions != 1) {
        // test failure -- detach threads in this case so process can proceed without hanging
        thread1.detach();
        thread2.detach();
        thread3.detach();
    } else {
        thread1.join();
        thread2.join();
        thread3.join();
    }
    BOOST_CHECK(ctx->lock_exceptions == 1);
}


// Identical to test8, but uses a RecursiveMutex instead (deadlock should still be detected)
BOOST_AUTO_TEST_CASE(test8r) {
    struct Context {
        RecursiveMutex mutexA;
        RecursiveMutex mutexB;
        RecursiveMutex mutexC;

        std::atomic<bool> done{false};
        std::atomic<int> lock_exceptions{0};
        std::atomic<int> writelocks{0};
    };
    using SharedCtx = std::shared_ptr<Context>;
    SharedCtx ctx = std::make_shared<Context>();

    auto Thread1 = [](SharedCtx ctx){
        LOCK(ctx->mutexA); // 1
        ++ctx->writelocks;
        while (ctx->writelocks != 3) ;
        try {
            LOCK(ctx->mutexB);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    auto Thread2 = [](SharedCtx ctx){
        while (ctx->writelocks != 1) ;
        LOCK(ctx->mutexB); // 2
        ++ctx->writelocks;
        while (ctx->writelocks != 3) ;
        try {
            LOCK(ctx->mutexC);
        } catch (const PotentialDeadlockError&) {
            ctx->lock_exceptions++;
        }
        while (!ctx->done) ;
    };

    auto Thread3 = [](SharedCtx ctx){
        while (ctx->writelocks != 2) ;
        LOCK(ctx->mutexC);
        ++ctx->writelocks;
        while (ctx->writelocks != 3) ;
        try {
            LOCK(ctx->mutexA);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };

    std::thread thread1(Thread1, ctx);
    std::thread thread2(Thread2, ctx);
    std::thread thread3(Thread3, ctx);
    Tic elapsed;
    while (!ctx->lock_exceptions && elapsed.secs() < 5) ;
    ctx->done = true;
    if (ctx->lock_exceptions != 1) {
        // test failure -- detach threads in this case so process can proceed without hanging
        thread1.detach();
        thread2.detach();
        thread3.detach();
    } else {
        thread1.join();
        thread2.join();
        thread3.join();
    }
    BOOST_CHECK(ctx->lock_exceptions == 1);
}


// 2 shared mutex lock themselves then try to
// shared lock each other
// this should deadlock and throw an exception
// this is the same as test 5 but we are using pointers to mutex
// instead of global mutex, this is an important difference
BOOST_AUTO_TEST_CASE(test9) {
    struct Context {
        SharedMutex mutex[2];
        std::atomic<bool> done{false};
        std::atomic<int> lock_exceptions{0};
        std::atomic<int> writelocks{0};
    };
    using SharedCtx = std::shared_ptr<Context>;
    SharedCtx ctx = std::make_shared<Context>();

    auto TestThread = [](SharedCtx ctx, SharedMutex *mutexA, SharedMutex *mutexB) {
        LOCK(*mutexA);
        ++ctx->writelocks;
        while (ctx->writelocks != 2) ;
        try {
            LOCK_SHARED(*mutexB);
        } catch (const PotentialDeadlockError&) {
            ++ctx->lock_exceptions;
        }
        while (!ctx->done) ;
    };
    std::thread thread1(TestThread, ctx, &ctx->mutex[0], &ctx->mutex[1]);
    std::thread thread2(TestThread, ctx, &ctx->mutex[1], &ctx->mutex[0]);
    Tic elapsed;
    while (!ctx->lock_exceptions && elapsed.secs() < 5) ;
    ctx->done = true;
    if (ctx->lock_exceptions != 1) {
        // test failure -- detach threads in this case so process can proceed without hanging
        thread1.detach();
        thread2.detach();
    } else {
        thread1.join();
        thread2.join();
    }
    BOOST_CHECK(ctx->lock_exceptions == 1);
}


// a very basic test to test lock order history tracking
// this should error because a different lock ordering was previously seen
BOOST_AUTO_TEST_CASE(test10) {
    SharedMutex mutexA;
    SharedMutex mutexB;

    auto Thread1 = [&]{
        LOCK(mutexA);
        LOCK(mutexB);
    };

    std::atomic_bool didThrow{false};
    auto Thread2 = [&]{
        LOCK(mutexB);
        try {
            LOCK(mutexA);
        } catch (const PotentialDeadlockError &) {
            didThrow = true;
        }
    };

    std::thread thread1(Thread1);
    thread1.join();
    std::thread thread2(Thread2);
    thread2.join();
    BOOST_CHECK(didThrow);
}

// RecursiveMutex version of test10
// this should error because a different lock ordering was previously seen
BOOST_AUTO_TEST_CASE(test10r) {
    RecursiveMutex mutexA;
    RecursiveMutex mutexB;

    auto Thread1 = [&]{
        LOCK(mutexA);
        LOCK(mutexB);
    };

    std::atomic_bool didThrow{false};
    auto Thread2 = [&]{
        LOCK(mutexB);
        try {
            LOCK(mutexA);
        } catch (const PotentialDeadlockError &) {
            didThrow = true;
        }
    };

    std::thread thread1(Thread1);
    thread1.join();
    std::thread thread2(Thread2);
    thread2.join();
    BOOST_CHECK(didThrow);
}

// A test helper that checks that destroying a lock and creating another one
// in the same memory location should lead to a situation where there *is* no
// deadlock detected.  This is because on lock destruction, lock histories
// should be cleared for that lock (since it's "going away"!).
template <typename MutexT>
void test11_generic() {
    std::optional<MutexT> mutexA;
    std::optional<MutexT> mutexB;

    for (auto *lockPtr : {&mutexA, &mutexB}) {
        // (Re)construct locks (wipe history for this run)
        mutexA.emplace();
        mutexB.emplace();

        auto Thread1 = [&]{
            LOCK(mutexA.value());
            LOCK(mutexB.value());
        };

        std::atomic_bool didThrow{false};
        auto Thread2 = [&]{
            LOCK(mutexB.value());
            try {
                LOCK(mutexA.value());
            } catch (const PotentialDeadlockError &) {
                didThrow = true;
            }
        };

        std::thread thread1(Thread1);
        thread1.join();

        // Now, delete and recreate one of the locks at the same memory
        // location (lock history for it should be wiped).
        lockPtr->reset();
        lockPtr->emplace();

        // Even though we are locking B then A in the below thread, one of
        // them is "new" (was reconstructed above) and thus it has no
        // lock order history, and so no deadlock should be detected.
        std::thread thread2(Thread2);
        thread2.join();

        BOOST_CHECK(!didThrow);
    }
}

BOOST_AUTO_TEST_CASE(test11) {
    // Run the above test for all 3 lock types
    test11_generic<RecursiveMutex>();
    test11_generic<Mutex>();
    test11_generic<SharedMutex>();
}

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6))
#pragma GCC diagnostic pop
#endif

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#else // !DEBUG_LOCKORDER || SKIP_SANITIZER_NOT_SUPPORTED

BOOST_AUTO_TEST_CASE(empty_deadlock_tests) {
    BOOST_CHECK("Compile in Debug mode (without sanitize=thread), to enable the deadlock_tests");
}

#endif

BOOST_AUTO_TEST_SUITE_END()
