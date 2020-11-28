// Copyright (c) 2017-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_BLOCKCHAIN_H
#define BITCOIN_RPC_BLOCKCHAIN_H

#include <univalue.h>
#include <vector>
#include <stdint.h>
#include <amount.h>

class CBlock;
class CBlockIndex;
class Config;
class CTxMemPool;
class JSONRPCRequest;

UniValue getblockchaininfo(const Config &config, const JSONRPCRequest &request);

static constexpr int NUM_GETBLOCKSTATS_PERCENTILES = 5;

/**
 * Get the required difficulty of the next block w/r/t the given block index.
 *
 * @return A floating point number that is a multiple of the main net minimum
 * difficulty (4295032833 hashes).
 */
double GetDifficulty(const CBlockIndex *blockindex);

/** Callback for when block tip changed. */
void RPCNotifyBlockChange(bool ibd, const CBlockIndex *pindex);

/** Block description to JSON */
UniValue::Object blockToJSON(const Config &config, const CBlock &block, const CBlockIndex *tip, const CBlockIndex *blockindex, bool txDetails = false);

/** Mempool information to JSON */
UniValue::Object MempoolInfoToJSON(const Config &config, const CTxMemPool &pool);

/** Mempool to JSON */
UniValue MempoolToJSON(const CTxMemPool &pool, bool verbose = false);

/** Block header to JSON */
UniValue::Object blockheaderToJSON(const CBlockIndex *tip, const CBlockIndex *blockindex);

/** Used by getblockstats to get feerates at different percentiles by weight  */
void CalculatePercentilesBySize(Amount result[NUM_GETBLOCKSTATS_PERCENTILES], std::vector<std::pair<Amount, int64_t>>& scores, int64_t total_size);

#endif // BITCOIN_RPC_BLOCKCHAIN_H
