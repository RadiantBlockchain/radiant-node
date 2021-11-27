// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <sync.h>
#include <tinyformat.h>

#include <logging.h>
#include <util/strencodings.h>
#include <util/threadnames.h>

#include <cstdio>
#include <functional>
#include <map>
#include <set>
#include <system_error>
#include <vector>

#ifdef DEBUG_LOCKCONTENTION
void PrintLockContention(const char *pszName, const char *pszFile, int nLine) {
    LogPrintf("LOCKCONTENTION: %s\n", pszName);
    LogPrintf("Locker: %s:%d\n", pszFile, nLine);
}
#endif /* DEBUG_LOCKCONTENTION */

#ifdef DEBUG_LOCKORDER
//
// Early deadlock detection.
// Problem being solved:
//    Thread 1 locks A, then B, then C
//    Thread 2 locks D, then C, then A
//     --> may result in deadlock between the two threads, depending on when
//     they run.
// Solution implemented here:
// Keep track of pairs of locks: (A before B), (A before C), etc.
// Complain if any thread tries to lock in a different order.
//

struct CLockLocation {
    CLockLocation(
        const char* pszName,
        const char* pszFile,
        int nLine,
        bool fTryIn,
        const std::string& thread_name,
        bool fRecursiveIn)
        : fTry(fTryIn),
          mutexName(pszName),
          sourceFile(pszFile),
          m_thread_name(thread_name),
          sourceLine(nLine),
          fRecursive(fRecursiveIn) {}

    std::string ToString() const
    {
        return tfm::format(
            "%s %s:%s%s%s (in thread %s)",
            mutexName, sourceFile, itostr(sourceLine), (fTry ? " (TRY)" : ""), (fRecursive ? " (RECURSIVE)": ""),
            m_thread_name);
    }

    const std::string & Name() const { return mutexName; }

    bool IsRecursive() const { return fRecursive; }

private:
    bool fTry;
    std::string mutexName;
    std::string sourceFile;
    const std::string m_thread_name;
    int sourceLine;
    bool fRecursive;
};

typedef std::vector<std::pair<void *, CLockLocation>> LockStack;
typedef std::map<std::pair<void *, void *>, LockStack> LockOrders;
typedef std::set<std::pair<void *, void *>> InvLockOrders;

struct LockData {
    // Very ugly hack: as the global constructs and destructors run single
    // threaded, we use this boolean to know whether LockData still exists,
    // as DeleteLock can get called by global RecursiveMutex destructors
    // after LockData disappears.
    bool available;
    LockData() : available(true) {}
    ~LockData() { available = false; }

    LockOrders lockorders;
    InvLockOrders invlockorders;
    std::mutex dd_mutex;

    /// For cycle detection: given a lock, the set of locks that were ever locked before it
    std::set<void *> getParentsOf(void *cs) const;
};

std::set<void *> LockData::getParentsOf(void *cs) const {
    std::set<void *> ret;
    for (auto it = invlockorders.lower_bound({cs, nullptr}); it != invlockorders.end() && it->first == cs; ++it) {
        ret.emplace_hint(ret.end(), it->second);
    }
    return ret;
}

LockData &GetLockData() {
    static LockData lockdata;
    return lockdata;
}

static thread_local LockStack g_lockstack;

static void
potential_deadlock_detected(const std::pair<void *, void *> &mismatch,
                            const LockStack &s1, const LockStack &s2) {
    std::vector<void *> ids;
    if (!g_debug_lockorder_abort) ids.reserve(4);
    LogPrintf("POTENTIAL DEADLOCK DETECTED\n");
    LogPrintf("Previous lock order was:\n");
    for (const std::pair<void *, CLockLocation> &i : s2) {
        if (i.first == mismatch.first) {
            LogPrintfToBeContinued(" (1)");
            if (!g_debug_lockorder_abort) ids.emplace_back(i.first);
        }
        if (i.first == mismatch.second) {
            LogPrintfToBeContinued(" (2)");
            if (!g_debug_lockorder_abort) ids.emplace_back(i.first);
        }
        LogPrintf(" %s\n", i.second.ToString());
    }
    LogPrintf("Current lock order is:\n");
    for (const std::pair<void *, CLockLocation> &i : s1) {
        if (i.first == mismatch.first) {
            LogPrintfToBeContinued(" (1)");
            if (!g_debug_lockorder_abort) ids.emplace_back(i.first);
        }
        if (i.first == mismatch.second) {
            LogPrintfToBeContinued(" (2)");
            if (!g_debug_lockorder_abort) ids.emplace_back(i.first);
        }
        LogPrintf(" %s\n", i.second.ToString());
    }
    if (g_debug_lockorder_abort) {
        fprintf(stderr,
                "Assertion failed: detected inconsistent lock order at %s:%i, "
                "details in debug log.\n",
                __FILE__, __LINE__);
        abort();
    }
    throw PotentialDeadlockError("potential deadlock detected",
                                 {ids.size() > 0 ? ids[0] : nullptr, ids.size() > 1 ? ids[1] : nullptr},
                                 {ids.size() > 2 ? ids[2] : nullptr, ids.size() > 3 ? ids[3] : nullptr});
}

static void
potential_self_deadlock_detected(const CLockLocation &cur, const CLockLocation &prev, void *pcur) {
    LogPrintf("POTENTIAL SELF-DEADLOCK DETECTED\n");
    LogPrintf("Current locking location: %s, previous lock location: %s\n", cur.ToString(), prev.ToString());
    if (g_debug_lockorder_abort) {
        fprintf(stderr,
                "Assertion failed: detected thread that deadlocks itself at %s:%i [%s], "
                "details in debug log.\n",
                __FILE__, __LINE__, cur.ToString().c_str());
        abort();
    }
    throw PotentialDeadlockError("potential deadlock detected", {pcur, pcur}, {pcur, pcur});
}

static void
potential_deadlock_cycle_detected(const CLockLocation &cur, void *l1, void *l2, void *cs) {
    LogPrintf("POTENTIAL DEADLOCK CYCLE DETECTED\n");
    LogPrintf("Current locking location: %s\n", cur.ToString());
    if (g_debug_lockorder_abort) {
        fprintf(stderr,
                "Assertion failed: detected a potentially deadlicking lock cycle at %s:%i [%s], "
                "details in debug log.\n",
                __FILE__, __LINE__, cur.ToString().c_str());
        abort();
    }
    throw PotentialDeadlockError("potential deadlock detected", {l1, l2}, {cs, l1});
}

static void push_lock(void *c, const CLockLocation &locklocation) {
    LockData &lockdata = GetLockData();
    std::lock_guard<std::mutex> lock(lockdata.dd_mutex);

    g_lockstack.push_back(std::make_pair(c, locklocation));

    size_t iterCt = 0;
    for (const std::pair<void *, CLockLocation> &i : g_lockstack) {
        ++iterCt;
        if (i.first == c) {
            if (iterCt == g_lockstack.size() || locklocation.IsRecursive()) {
                break;
            } else {
                // disallow re-lock of a non-recursive lock
                potential_self_deadlock_detected(locklocation, i.second, c);
            }
        }

        std::pair<void *, void *> p1 = std::make_pair(i.first, c);
        if ( ! lockdata.lockorders.try_emplace(p1, g_lockstack).second) {
            continue;
        }

        std::pair<void *, void *> p2 = std::make_pair(c, i.first);
        lockdata.invlockorders.insert(p2);
        if (lockdata.lockorders.count(p2)) {
            potential_deadlock_detected(p1, lockdata.lockorders[p1], lockdata.lockorders[p2]);
        }
    }

    // Try to find cycles where:
    // Thread1: Locks A, B
    // Thread2: Locks B, C
    // Thread3: Locks C, A
    std::set<void *> seen;
    std::function<void(void *)> Recurse = [&](void *cur) {
        if ( ! seen.insert(cur).second) return;
        // If the lock `c` is an "ancestor" of itself, then we have detected a deadlock cycle.
        // Note: the way the first loop above is written, a recursive lock will never be modeled
        // as its own ancestor under normal, non-deadlocking usage patterns.
        for (void *parent : lockdata.getParentsOf(cur)) {
            if (lockdata.getParentsOf(parent).count(c)) {
                potential_deadlock_cycle_detected(locklocation, parent, cur, c);
            }
            Recurse(parent);
        }
    };
    Recurse(c);
}

static void pop_lock() {
    g_lockstack.pop_back();
}

void EnterCritical(const char* pszName, const char* pszFile, int nLine, void *cs, bool fTry, bool fRecursive)
{
    try {
        push_lock(cs, CLockLocation(pszName, pszFile, nLine, fTry, util::ThreadGetInternalName(), fRecursive));
    } catch (const PotentialDeadlockError &) {
        // we must undo the lock stack push since the lock won't be acquired (this fixes unit tests)
        pop_lock();
        throw;
    }
}

void CheckLastCritical(void *cs, std::string &lockname, const char *guardname,
                       const char *file, int line) {
    if (!g_lockstack.empty()) {
        const auto &lastlock = g_lockstack.back();
        if (lastlock.first == cs) {
            lockname = lastlock.second.Name();
            return;
        }
    }
    throw std::system_error(
        EPERM, std::generic_category(),
        strprintf("%s:%s %s was not most recent critical section locked", file,
                  line, guardname));
}

void LeaveCritical() {
    pop_lock();
}

std::string LocksHeld() {
    std::string result;
    for (const std::pair<void *, CLockLocation> &i : g_lockstack) {
        result += i.second.ToString() + std::string("\n");
    }
    return result;
}

void AssertLockHeldInternal(const char *pszName, const char *pszFile, int nLine,
                            void *cs) {
    for (const std::pair<void *, CLockLocation> &i : g_lockstack) {
        if (i.first == cs) {
            return;
        }
    }
    fprintf(stderr,
            "Assertion failed: lock %s not held in %s:%i; locks held:\n%s",
            pszName, pszFile, nLine, LocksHeld().c_str());
    abort();
}

void AssertLockNotHeldInternal(const char *pszName, const char *pszFile,
                               int nLine, void *cs) {
    for (const std::pair<void *, CLockLocation> &i : g_lockstack) {
        if (i.first == cs) {
            fprintf(stderr,
                    "Assertion failed: lock %s held in %s:%i; locks held:\n%s",
                    pszName, pszFile, nLine, LocksHeld().c_str());
            abort();
        }
    }
}

void DeleteLock(void *cs) {
    LockData &lockdata = GetLockData();
    if (!lockdata.available) {
        // We're already shutting down.
        return;
    }

    std::lock_guard<std::mutex> lock(lockdata.dd_mutex);
    std::pair<void *, void *> item = std::make_pair(cs, nullptr);
    LockOrders::iterator it = lockdata.lockorders.lower_bound(item);
    while (it != lockdata.lockorders.end() && it->first.first == cs) {
        std::pair<void *, void *> invitem =
            std::make_pair(it->first.second, it->first.first);
        lockdata.invlockorders.erase(invitem);
        lockdata.lockorders.erase(it++);
    }
    InvLockOrders::iterator invit = lockdata.invlockorders.lower_bound(item);
    while (invit != lockdata.invlockorders.end() && invit->first == cs) {
        std::pair<void *, void *> invinvitem =
            std::make_pair(invit->second, invit->first);
        lockdata.lockorders.erase(invinvitem);
        lockdata.invlockorders.erase(invit++);
    }
}

bool g_debug_lockorder_abort = true;
PotentialDeadlockError::~PotentialDeadlockError() {}

#endif /* DEBUG_LOCKORDER */
