// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MEMPOOL_BATCHUPDATER
#define BITCOIN_MEMPOOL_BATCHUPDATER

#include <primitives/transaction.h>

#include <memory>
#include <vector>

namespace mempool {

/**
 * Interface for doing large operations on the mempool, such as updating the
 * mempool after a new block has connected.
 *
 * This interface exists as to allow for exploring alternative algorithms,
 * while being able to default to the stable (and slower) one.
 */
class BatchUpdater {
public:
    virtual ~BatchUpdater() = 0;

    /**
     * Called when a block is connected. Removes transactions from mempool.
     *
     * Requires lock held on mempool.cs
     */
    virtual void removeForBlock(const std::vector<CTransactionRef> &vtx,
                                uint64_t nBlockHeight) = 0;
};

// Linker error without this default implementation
inline BatchUpdater::~BatchUpdater() { }
}

#endif
