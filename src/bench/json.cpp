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
#include <rpc/protocol.h>

#include <univalue.h>

static void JSONReadWriteBlock(const std::vector<uint8_t> &data, unsigned int pretty, bool write, benchmark::State &state) {
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
    const auto blockuv = blockToJSON(GetConfig(), block, &blockindex, &blockindex, /*verbose*/ true);

    if (write) {
        while (state.KeepRunning()) {
            (void)UniValue::stringify(blockuv, pretty);
        }
    } else {
        std::string json = UniValue::stringify(blockuv, pretty);
        while (state.KeepRunning()) {
            UniValue uv;
            if (!uv.read(json))
                throw std::runtime_error("UniValue lib failed to parse its own generated string.");
        }
    }
}

static void JSONReadBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(benchmark::data::Get_block413567(), 0, false, state);
}
static void JSONReadBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(benchmark::data::Get_block556034(), 0, false, state);
}
static void JSONWriteBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(benchmark::data::Get_block413567(), 0, true, state);
}
static void JSONWriteBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(benchmark::data::Get_block556034(), 0, true, state);
}
static void JSONWritePrettyBlock_1MB(benchmark::State &state) {
    JSONReadWriteBlock(benchmark::data::Get_block413567(), 4, true, state);
}
static void JSONWritePrettyBlock_32MB(benchmark::State &state) {
    JSONReadWriteBlock(benchmark::data::Get_block556034(), 4, true, state);
}

BENCHMARK(JSONReadBlock_1MB, 18);
BENCHMARK(JSONReadBlock_32MB, 1);
BENCHMARK(JSONWriteBlock_1MB, 52);
BENCHMARK(JSONWriteBlock_32MB, 1);
BENCHMARK(JSONWritePrettyBlock_1MB, 47);
BENCHMARK(JSONWritePrettyBlock_32MB, 1);
