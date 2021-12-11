// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <threadsafety.h>

#include <condition_variable>
#include <mutex>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>

/////////////////////////////////////////////////
//                                             //
// THE SIMPLE DEFINITION, EXCLUDING DEBUG CODE //
//                                             //
/////////////////////////////////////////////////

/*

Mutex mutex;
    std::mutex mutex;

SharedMutex mutex;
    std::shared_mutex mutex;

RecursiveMutex mutex;
    std::recursive_mutex mutex;

LOCK(mutex);
    std::unique_lock criticalblock(mutex);

LOCK_SHARED(mutex);
    std::shared_lock<std::shared_mutex> criticalblock(mutex);

LOCK2(mutex1, mutex2);
    std::unique_lock criticalblock1(mutex1);
    std::unique_lock criticalblock2(mutex2);

TRY_LOCK(mutex, name);
    std::unique_lock< name(mutex, std::try_to_lock_t);

TRY_LOCK_SHARED(mutex, name);
    std::shared_lock<std::shared_mutex> name(mutex, std::try_to_lock_t);

ENTER_CRITICAL_SECTION(mutex); // no RAII
    mutex.lock();

LEAVE_CRITICAL_SECTION(mutex); // no RAII
    mutex.unlock();
 */

///////////////////////////////
//                           //
// THE ACTUAL IMPLEMENTATION //
//                           //
///////////////////////////////

#ifdef DEBUG_LOCKORDER
void EnterCritical(const char *pszName, const char *pszFile, int nLine,
                   void *cs, bool fTry = false, bool fRecursive = false);
void LeaveCritical();
void CheckLastCritical(void *cs, std::string &lockname, const char *guardname,
                       const char *file, int line);
std::string LocksHeld();
void AssertLockHeldInternal(const char *pszName, const char *pszFile, int nLine,
                            void *cs) ASSERT_EXCLUSIVE_LOCK(cs);
void AssertLockNotHeldInternal(const char *pszName, const char *pszFile,
                               int nLine, void *cs);
void DeleteLock(void *cs);

/**
 * Call abort() if a potential lock order deadlock bug is detected, instead of
 * just logging information and throwing a logic_error. Defaults to true, and
 * set to false in DEBUG_LOCKORDER unit tests.
 */
extern bool g_debug_lockorder_abort;

/**
 *  This exception is thrown if g_debug_lockorder_abort == false, and if there
 *  is a potential deadlock detected in LOCK() and friends.
 */
struct PotentialDeadlockError : std::logic_error {
    using LockPtrPair = std::pair<void *, void *>;
    PotentialDeadlockError(const char *message, const LockPtrPair &prev, const LockPtrPair &cur)
        : std::logic_error(message), prevOrder(prev), curOrder(cur) {}
    ~PotentialDeadlockError();

    LockPtrPair prevOrder; ///< addresses of the mismatching locks (in the order previously seen)
    LockPtrPair curOrder; ///< addresses of the mismatching locks (in the order as currently encountered)
};
#else
static inline void EnterCritical(const char *pszName, const char *pszFile,
                                 int nLine, void *cs, bool fTry = false, bool fRecursive = false) {}
static inline void LeaveCritical() {}
static inline void CheckLastCritical(void *cs, std::string &lockname,
                                     const char *guardname, const char *file,
                                     int line) {}
static inline void AssertLockHeldInternal(const char *pszName,
                                          const char *pszFile, int nLine,
                                          void *cs) ASSERT_EXCLUSIVE_LOCK(cs) {}
static inline void AssertLockNotHeldInternal(const char *pszName,
                                             const char *pszFile, int nLine,
                                             void *cs) {}
static inline void DeleteLock(void *cs) {}
#endif
#define AssertLockHeld(cs) AssertLockHeldInternal(#cs, __FILE__, __LINE__, &cs)
#define AssertLockNotHeld(cs)                                                  \
    AssertLockNotHeldInternal(#cs, __FILE__, __LINE__, &cs)

/**
 * Template mixin that adds -Wthread-safety locking annotations and lock order
 * checking to a subset of the mutex API.
 */
template <typename PARENT> struct LOCKABLE AnnotatedMixin : PARENT {
    static constexpr bool recursive = std::is_base_of_v<std::recursive_mutex, PARENT>;

    ~AnnotatedMixin() { DeleteLock((void *)this); }

    void lock() EXCLUSIVE_LOCK_FUNCTION() { PARENT::lock(); }

    void unlock() UNLOCK_FUNCTION() { PARENT::unlock(); }

    bool try_lock() EXCLUSIVE_TRYLOCK_FUNCTION(true) {
        return PARENT::try_lock();
    }

    using UniqueLock = std::unique_lock<PARENT>;
};

template <typename PARENT> struct LOCKABLE AnnotatedSharedMixin : AnnotatedMixin<PARENT> {
    void lock_shared() SHARED_LOCK_FUNCTION() { PARENT::lock_shared(); }
    void unlock_shared() UNLOCK_FUNCTION() { PARENT::unlock_shared(); }
    bool try_lock_shared() SHARED_TRYLOCK_FUNCTION(true) { return PARENT::try_lock_shared(); }
    using SharedLock = std::shared_lock<PARENT>;
};

/**
 * Wrapped mutex: supports recursive locking, but no waiting
 * TODO: We should move away from using the recursive lock by default.
 */
using RecursiveMutex = AnnotatedMixin<std::recursive_mutex>;

/** Wrapped mutex: supports waiting but not recursive locking */
using Mutex = AnnotatedMixin<std::mutex>;

/** Wrapped shared_mutex: supports multiple readers, one writer (read-write locking) */
using SharedMutex = AnnotatedSharedMixin<std::shared_mutex>;

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char *pszName, const char *pszFile, int nLine);
#endif

/** Mixin class that does the low-level work of notifying our lock debug system. Base of: SharedLock and UniqueLock. */
template <typename Base, bool recursive>
class EnterMixin : public Base {
    void Enter(const char *pszName, const char *pszFile, int nLine) {
        EnterCritical(pszName, pszFile, nLine, (void *)(Base::mutex()), false /* try */, recursive);
#ifdef DEBUG_LOCKCONTENTION
        if (!Base::try_lock()) {
            PrintLockContention(pszName, pszFile, nLine);
#endif
            Base::lock();
#ifdef DEBUG_LOCKCONTENTION
        }
#endif
    }

    bool TryEnter(const char *pszName, const char *pszFile, int nLine) {
        EnterCritical(pszName, pszFile, nLine, (void *)(Base::mutex()), true /* try */, recursive);
        Base::try_lock();
        if (!Base::owns_lock()) {
            LeaveCritical();
        }
        return Base::owns_lock();
    }

protected:
    EnterMixin(typename Base::mutex_type &mutexIn, const char *pszName, const char *pszFile, int nLine, bool fTry)
        : Base(mutexIn, std::defer_lock) {
        if (fTry) {
            TryEnter(pszName, pszFile, nLine);
        } else {
            Enter(pszName, pszFile, nLine);
        }
    }

    ~EnterMixin() {
        if (Base::owns_lock()) {
            LeaveCritical();
        }
    }

public:
    operator bool() { return Base::owns_lock(); }

protected:
    // needed for reverse_lock
    EnterMixin() {}

public:
    /**
     * An RAII-style reverse lock. Unlocks on construction and locks on
     * destruction.
     */
    class reverse_lock {
        EnterMixin &lock;
        EnterMixin templock;
        std::string lockname;
        const std::string file;
        const int line;

    public:
        explicit reverse_lock(EnterMixin &_lock, const char *_guardname,
                              const char *_file, int _line)
            : lock(_lock), file(_file), line(_line) {
            CheckLastCritical((void *)lock.mutex(), lockname, _guardname, _file,
                              _line);
            lock.unlock();
            LeaveCritical();
            lock.swap(templock);
        }

        reverse_lock(reverse_lock const&) = delete;
        reverse_lock& operator=(reverse_lock const&) = delete;

        ~reverse_lock() {
            templock.swap(lock);
            EnterCritical(lockname.c_str(), file.c_str(), line,
                          (void *)lock.mutex(), false /* try */, recursive);
            lock.lock();
        }
    };
    friend class reverse_lock;
};

#define REVERSE_LOCK(g)                                                        \
    decltype(g)::reverse_lock PASTE2(revlock, __COUNTER__)(g, #g, __FILE__,    \
                                                           __LINE__)

/** Wrapper around std::unique_lock style lock for Mutex. */
template <typename Mutex, typename Base = typename Mutex::UniqueLock>
struct SCOPED_LOCKABLE UniqueLock : EnterMixin<Base, Mutex::recursive> {
    UniqueLock(Mutex &mutexIn, const char *pszName, const char *pszFile,
               int nLine, bool fTry = false) EXCLUSIVE_LOCK_FUNCTION(mutexIn)
        : EnterMixin<Base, Mutex::recursive>(mutexIn, pszName, pszFile, nLine, fTry) {}

    ~UniqueLock() UNLOCK_FUNCTION() {}
};

/** Wrapper around std::shared_lock style lock for SharedMutex. */
template <typename SharedMutex, typename Base = typename SharedMutex::SharedLock>
struct SCOPED_LOCKABLE SharedLock : EnterMixin<Base, SharedMutex::recursive> {
    SharedLock(SharedMutex &mutexIn, const char *pszName, const char *pszFile,
               int nLine, bool fTry = false) SHARED_LOCK_FUNCTION(mutexIn)
        : EnterMixin<Base, SharedMutex::recursive>(mutexIn, pszName, pszFile, nLine, fTry) {}

    ~SharedLock() UNLOCK_FUNCTION() {}
};

template <typename MutexArg>
using DebugLock = UniqueLock<std::remove_reference_t<std::remove_pointer_t<MutexArg>>>;

template <typename SharedMutexArg>
using DebugSharedLock = SharedLock<std::remove_reference_t<std::remove_pointer_t<SharedMutexArg>>>;

#define PASTE(x, y) x##y
#define PASTE2(x, y) PASTE(x, y)

#define LOCK(cs)                                                               \
    DebugLock<decltype(cs)> PASTE2(criticalblock, __COUNTER__)                 \
                                (cs, #cs, __FILE__, __LINE__)
#define LOCK_SHARED(cs)                                                        \
    DebugSharedLock<decltype(cs)> PASTE2(criticalblock, __COUNTER__)           \
                                      (cs, #cs, __FILE__, __LINE__)
#define LOCK2(cs1, cs2)                                                        \
    DebugLock<decltype(cs1)> criticalblock1(cs1, #cs1, __FILE__, __LINE__);    \
    DebugLock<decltype(cs2)> criticalblock2(cs2, #cs2, __FILE__, __LINE__);
#define TRY_LOCK(cs, name)                                                     \
    DebugLock<decltype(cs)> name(cs, #cs, __FILE__, __LINE__, true)
#define TRY_LOCK_SHARED(cs, name)                                              \
    DebugSharedLock<decltype(cs)> name(cs, #cs, __FILE__, __LINE__, true)
#define WAIT_LOCK(cs, name)                                                    \
    DebugLock<decltype(cs)> name(cs, #cs, __FILE__, __LINE__)
#define WAIT_LOCK_SHARED(cs, name)                                             \
    DebugSharedLock<decltype(cs)> name(cs, #cs, __FILE__, __LINE__)

#define ENTER_CRITICAL_SECTION(cs)                                             \
    {                                                                          \
        EnterCritical(#cs, __FILE__, __LINE__, (void *)(&cs),                  \
                      false /* try */, (cs).recursive);                        \
        (cs).lock();                                                           \
    }

#define LEAVE_CRITICAL_SECTION(cs)                                             \
    {                                                                          \
        (cs).unlock();                                                         \
        LeaveCritical();                                                       \
    }

//! Run code while locking a mutex.
//!
//! Examples:
//!
//!   WITH_LOCK(cs, shared_val = shared_val + 1);
//!
//!   int val = WITH_LOCK(cs, return shared_val);
//!
//! Note:
//!
//! Since the return type deduction follows that of decltype(auto), while the
//! deduced type of:
//!
//!   WITH_LOCK(cs, return {int i = 1; return i;});
//!
//! is int, the deduced type of:
//!
//!   WITH_LOCK(cs, return {int j = 1; return (j);});
//!
//! is &int, a reference to a local variable
//!
//! The above is detectable at compile-time with the -Wreturn-local-addr flag in
//! gcc and the -Wreturn-stack-address flag in clang, both enabled by default.
#define WITH_LOCK(cs, code) [&]() -> decltype(auto) { LOCK(cs); code; }()

class CSemaphore {
    std::condition_variable condition;
    std::mutex mutex;
    int value;

public:
    explicit CSemaphore(int init) : value(init) {}

    void wait() {
        std::unique_lock<std::mutex> lock(mutex);
        condition.wait(lock, [&]() { return value >= 1; });
        value--;
    }

    bool try_wait() {
        std::lock_guard<std::mutex> lock(mutex);
        if (value < 1) {
            return false;
        }
        value--;
        return true;
    }

    void post() {
        {
            std::lock_guard<std::mutex> lock(mutex);
            value++;
        }
        condition.notify_one();
    }
};

/** RAII-style semaphore lock */
class CSemaphoreGrant {
    CSemaphore *sem;
    bool fHaveGrant;

public:
    void Acquire() {
        if (fHaveGrant) {
            return;
        }
        sem->wait();
        fHaveGrant = true;
    }

    void Release() {
        if (!fHaveGrant) {
            return;
        }
        sem->post();
        fHaveGrant = false;
    }

    bool TryAcquire() {
        if (!fHaveGrant && sem->try_wait()) {
            fHaveGrant = true;
        }
        return fHaveGrant;
    }

    void MoveTo(CSemaphoreGrant &grant) {
        grant.Release();
        grant.sem = sem;
        grant.fHaveGrant = fHaveGrant;
        fHaveGrant = false;
    }

    CSemaphoreGrant() : sem(nullptr), fHaveGrant(false) {}

    explicit CSemaphoreGrant(CSemaphore &sema, bool fTry = false)
        : sem(&sema), fHaveGrant(false) {
        if (fTry) {
            TryAcquire();
        } else {
            Acquire();
        }
    }

    ~CSemaphoreGrant() { Release(); }

    operator bool() const { return fHaveGrant; }
};
