// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>

#include <chainparams.h>
#include <config.h>
#include <consensus/validation.h>
#include <pow.h>
#include <streams.h>
#include <validation.h>

// These are the two major time-sinks which happen after we have fully received
// a block off the wire, but before we can relay the block on to peers using
// compact block relay.

static void DeserializeBlockTest(const std::vector<uint8_t> &data, benchmark::State &state) {
    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    while (state.KeepRunning()) {
        CBlock block;
        stream >> block;
        bool rewound = stream.Rewind(data.size());
        assert(rewound);
    }
}

static void DeserializeAndCheckBlockTest(const std::vector<uint8_t> &data, benchmark::State &state) {
    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    const Config &config = GetConfig();
    const Consensus::Params params = config.GetChainParams().GetConsensus();
    BlockValidationOptions options(config);
    while (state.KeepRunning()) {
        // Note that CBlock caches its checked state, so we need to recreate it
        // here.
        CBlock block;
        stream >> block;
        bool rewound = stream.Rewind(data.size());
        assert(rewound);

        CValidationState validationState;
        bool checked = CheckBlock(block, validationState, params, options);
        assert(checked);
    }
}

static void CheckBlockTest(const std::vector<uint8_t> &data, benchmark::State &state) {
    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    const Config &config = GetConfig();
    const Consensus::Params params = config.GetChainParams().GetConsensus();
    BlockValidationOptions options(config);
    CBlock block;
    stream >> block;
    CValidationState validationState;

    // de-serialize once, check many times
    while (state.KeepRunning()) {
        block.fChecked = false; // reset block checked state

        bool checked = CheckBlock(block, validationState, params, options);
        assert(checked);
    }
}

static void CheckProofOfWorkTest(const std::vector<uint8_t> &data, benchmark::State &state) {
    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    const Config &config = GetConfig();
    const Consensus::Params params = config.GetChainParams().GetConsensus();
    CBlock block;
    stream >> block;

    // de-serialize once, check many times
    while (state.KeepRunning()) {
        bool checked = CheckProofOfWork(block.GetHash(), block.nBits, params);
        assert(checked);
    }
}

static void CheckBlockHashTest(const std::vector<uint8_t> &data, benchmark::State &state, std::string const &expected_hash) {
    CDataStream stream(data, SER_NETWORK, PROTOCOL_VERSION);
    char a = '\0';
    stream.write(&a, 1); // Prevent compaction

    CBlock block;
    stream >> block;

    BlockHash const expected = BlockHash::fromHex(expected_hash);

    while (state.KeepRunning()) {
        BlockHash hash = block.GetHash();
        assert(hash == expected);
    }
}

static void DeserializeBlockTest_1MB(benchmark::State &state) {
    DeserializeBlockTest(benchmark::data::Get_block413567(), state);
}
static void DeserializeBlockTest_32MB(benchmark::State &state) {
    DeserializeBlockTest(benchmark::data::Get_block556034(), state);
}
static void DeserializeAndCheckBlockTest_1MB(benchmark::State &state) {
    DeserializeAndCheckBlockTest(benchmark::data::Get_block413567(), state);
}
static void DeserializeAndCheckBlockTest_32MB(benchmark::State &state) {
    DeserializeAndCheckBlockTest(benchmark::data::Get_block556034(), state);
}
static void CheckBlockTest_1MB(benchmark::State &state) {
    CheckBlockTest(benchmark::data::Get_block413567(), state);
}
static void CheckBlockTest_32MB(benchmark::State &state) {
    CheckBlockTest(benchmark::data::Get_block556034(), state);
}
static void CheckProofOfWorkTest_1MB(benchmark::State &state) {
    CheckProofOfWorkTest(benchmark::data::Get_block413567(), state);
}
static void CheckProofOfWorkTest_32MB(benchmark::State &state) {
    CheckProofOfWorkTest(benchmark::data::Get_block556034(), state);
}
static void CheckBlockHashTest_1MB(benchmark::State &state) {
    CheckBlockHashTest(benchmark::data::Get_block413567(), state, "0000000000000000025aff8be8a55df8f89c77296db6198f272d6577325d4069");
}
static void CheckBlockHashTest_32MB(benchmark::State &state) {
    CheckBlockHashTest(benchmark::data::Get_block556034(), state, "000000000000000000eb279368d5e158e5ef011010c98da89245f176e2083d64");
}

BENCHMARK(DeserializeBlockTest_1MB, 160);
BENCHMARK(DeserializeBlockTest_32MB, 3);
BENCHMARK(DeserializeAndCheckBlockTest_1MB, 130);
BENCHMARK(DeserializeAndCheckBlockTest_32MB, 2);
BENCHMARK(CheckBlockTest_1MB, 1600);
BENCHMARK(CheckBlockTest_32MB, 20);
BENCHMARK(CheckProofOfWorkTest_1MB, 1'000'000);
BENCHMARK(CheckProofOfWorkTest_32MB, 1'000'000);
BENCHMARK(CheckBlockHashTest_1MB, 1'000'000);
BENCHMARK(CheckBlockHashTest_32MB, 1'000'000);
