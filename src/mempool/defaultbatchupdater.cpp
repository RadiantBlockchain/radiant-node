// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <mempool/defaultbatchupdater.h>
#include <txmempool.h>
#include <reverse_iterator.h>

namespace mempool {

void DefaultBatchUpdater::removeForBlock(const std::vector<CTransactionRef> &vtx,
                                         uint64_t nBlockHeight [[maybe_unused]])
{
    AssertLockHeld(mempool.cs);
    DisconnectedBlockTransactions disconnectpool;
    disconnectpool.addForBlock(vtx);

    for (const CTransactionRef &tx :
         reverse_iterate(disconnectpool.GetQueuedTx().get<insertion_order>())) {
        CTxMemPool::txiter it = mempool.mapTx.find(tx->GetId());
        if (it != mempool.mapTx.end()) {
            CTxMemPool::setEntries stage;
            stage.insert(it);
            mempool.RemoveStaged(stage, MemPoolRemovalReason::BLOCK);
        }
        mempool.removeConflicts(*tx);
        mempool.ClearPrioritisation(tx->GetId());
    }

    disconnectpool.clear();
}

DefaultBatchUpdater::~DefaultBatchUpdater() { }

} // ns mempool
