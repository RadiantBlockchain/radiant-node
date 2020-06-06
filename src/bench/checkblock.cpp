// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <bench/data.h>

#include <chainparams.h>
#include <config.h>
#include <consensus/validation.h>
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

static void DeserializeBlockTest_1MB(benchmark::State &state) {
    DeserializeBlockTest(benchmark::data::block413567, state);
}
static void DeserializeBlockTest_32MB(benchmark::State &state) {
    DeserializeBlockTest(benchmark::data::block556034, state);
}
static void DeserializeAndCheckBlockTest_1MB(benchmark::State &state) {
    DeserializeAndCheckBlockTest(benchmark::data::block413567, state);
}
static void DeserializeAndCheckBlockTest_32MB(benchmark::State &state) {
    DeserializeAndCheckBlockTest(benchmark::data::block556034, state);
}

BENCHMARK(DeserializeBlockTest_1MB, 160);
BENCHMARK(DeserializeBlockTest_32MB, 3);
BENCHMARK(DeserializeAndCheckBlockTest_1MB, 130);
BENCHMARK(DeserializeAndCheckBlockTest_32MB, 2);
