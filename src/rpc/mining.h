// Copyright (c) 2017 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <gbtlight.h>
#include <primitives/transaction.h>
#include <script/script.h>

#include <univalue.h>

#include <memory>

class CBlock;
class Config;

/** Generate blocks (mine) */
UniValue generateBlocks(const Config &config,
                        std::shared_ptr<CReserveScript> coinbaseScript,
                        int nGenerate, uint64_t nMaxTries, bool keepScript);

namespace rpc {
/** Called by init code to register the internal "submitblock_StateCatcher" class. */
void RegisterSubmitBlockCatcher();
/** Called by shutdown code to delete the internal "submitblock_StateCatcher" class. */
void UnregisterSubmitBlockCatcher();
} // namespace rpc

namespace gbtl {
/** Used by getblocktemplatelight for the "merkle" UniValue entry it returns.  Returns a merkle branch used to
 *  reconstruct the merkle root for submitblocklight.  See the implementation of this function for more documentation.*/
std::vector<uint256> MakeMerkleBranch(std::vector<uint256> vtxHashes);
/** Used by submitblocklight.  Returns false if jobId is not in cache, otherwise returns true and puts the tx's for
 *  jobId into the specified block.
 *  Precondition: `block` should contain a single coinbase tx.
 *  Postcondition: On true return `block` contains its coinbase tx + the txs associated with jobId in consensus order.
 *                 On false return, `block` is not modified.
 *                 The merkle root for `block` is never modified by this function in either case.   */
bool GetTxsFromCache(const JobId &jobId, CBlock &block);
/** Used by submitblocklight.  Throws JSONRPCError on error, otherwise puts the tx data read from the jobId file into
 *  the specified block.
 *  Precondition: `block` should contain a single coinbase tx.
 *  Postcondition: If no exception is thrown, `block` contains its coinbase tx + the txs associated with jobId in
 *                 consensus order.  The merkle root for `block` is not modified by this function.   */
void LoadTxsFromFile(const JobId &jobId, CBlock &block);
/** Saves the tx's from pvtx (stripping the coinbase, if any) for jobId to the gJobIdTxCache and also to a disk file
 *  in GetJobDataDir().  submitblocklight will use these cached tx's later to reconstruct the transactions for a block.
 *  IMPORTANT: This function must be called with the cs_main lock held.  Test code should take cs_main before
 *             calling this function.   */
void CacheAndSaveTxsToFile(const JobId &jobId, const std::vector<CTransactionRef> *pvtx);
}
