// Copyright (c) 2016-2019 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>

#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <validation.h>
#include <streams.h>
#include <consensus/validation.h>
#include <rpc/blockchain.h>

#include <univalue.h>

static void RPCBlockVerbose(const std::vector<uint8_t> &data, benchmark::State &state) {
    SelectParams(CBaseChainParams::MAIN);

    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    CBlock block;
    stream >> block;

    CBlockIndex blockindex;
    const auto blockHash = block.GetHash();
    blockindex.phashBlock = &blockHash;
    blockindex.nBits = block.nBits;

    while (state.KeepRunning()) {
        (void)blockToJSON(GetConfig(), block, &blockindex, &blockindex, /*verbose*/ true);
    }
}

static void RPCBlockVerbose_1MB(benchmark::State &state) {
    RPCBlockVerbose(benchmark::data::Get_block413567(), state);
}
static void RPCBlockVerbose_32MB(benchmark::State &state) {
    RPCBlockVerbose(benchmark::data::Get_block556034(), state);
}

BENCHMARK(RPCBlockVerbose_1MB, 23);
BENCHMARK(RPCBlockVerbose_32MB, 1);
