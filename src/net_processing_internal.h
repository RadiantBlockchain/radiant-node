// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <net_nodeid.h>
#include <primitives/transaction.h>
#include <sync.h>

#include <map>
#include <set>

// `internal` namespace exposed *FOR TESTS ONLY*
// This namespace is for exposed internals not intended for public usage.
// We would ideally have made these private to the net_processing.cpp
// translation unit only, but since some tests need to see these functions
// (see denialofservice_tests.cpp), we do this instead.
namespace internal {

struct COrphanTx {
    const CTransactionRef tx;
    const NodeId fromPeer;
    const int64_t nTimeExpire;

    COrphanTx(const CTransactionRef &tx_, NodeId peer, int64_t expire)
        : tx(tx_), fromPeer(peer), nTimeExpire(expire) {}
};

extern RecursiveMutex g_cs_orphans;
using MapOrphanTransactions = std::map<TxId, COrphanTx>;
extern MapOrphanTransactions mapOrphanTransactions GUARDED_BY(g_cs_orphans);

//! Comparator by iter->first (TxId), ascending
struct IterTxidLess {
    using Iter = MapOrphanTransactions::iterator;
    bool operator()(const Iter &a, const Iter &b) const {
        return a->first < b->first;
    }
};
using MapOrphanTransactionsByPrev = std::map<COutPoint, std::set<MapOrphanTransactions::iterator, IterTxidLess>>;
//! Lookup by coin spent: every txin.prevout for every tx in mapOrphanTransactions has an entry in this map
extern MapOrphanTransactionsByPrev mapOrphanTransactionsByPrev GUARDED_BY(g_cs_orphans);

// Below are the 3 functions that manipulate mapOrphanTransactions and
// mapOrphanTransactionsByPrev (implemented in net_processing.cpp).
bool AddOrphanTx(const CTransactionRef &tx, NodeId peer) EXCLUSIVE_LOCKS_REQUIRED(g_cs_orphans);
void EraseOrphansFor(NodeId peer);
unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);

// This function is used for testing the stale tip eviction logic, see
// denialofservice_tests.cpp.
void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds);

} // namespace internal
