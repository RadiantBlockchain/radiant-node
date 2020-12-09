// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_MEMPOOL_DEFAULTBATCHUPDATER
#define BITCOIN_MEMPOOL_DEFAULTBATCHUPDATER

#include <mempool/batchupdater.h>

#include <vector>

class CTxMemPool;

namespace mempool {

class DefaultBatchUpdater : public mempool::BatchUpdater {
public:
    DefaultBatchUpdater(CTxMemPool& m) : mempool(m) { }

    void removeForBlock(const std::vector<CTransactionRef> &vtx,
                        uint64_t nBlockHeight) override;

    ~DefaultBatchUpdater() override;

private:
    CTxMemPool& mempool;
};

} // ns mempool

#endif
