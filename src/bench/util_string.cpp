// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>
#include <util/string.h>

#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

namespace {
std::string MakeStringSprinkledWithDelims(const size_t length, const size_t num_delims,
                                          const std::string &delims = " \f\n\r\t\v") {
    const std::string someChars = "thequickbrownfoxjumpedoverthelazydogsABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::string ret;

    // keep growing the string until it meets the minimal size requirement
    while (ret.size() + num_delims < length) {
        ret += someChars.substr(0, std::min<size_t>(someChars.size(), length - (ret.size() + num_delims)));
    }

    if (!delims.empty() && num_delims > 0) {
        // break string into equal pieces, inserting a delimiter from the delims set every piece_size chars.
        const size_t piece_size = ret.size() / num_delims;
        for (size_t i = 0; i < num_delims; ++i) {
            ret.insert((i + 1) * piece_size + i, std::string{1, delims[i % delims.size()]});
        }
    }

    // check sanity
    std::vector<std::string> toks;
    Split(toks, ret, delims);
    assert(toks.size() == num_delims + 1 || (delims.empty() && toks.size() == 1));

    return ret;
}

void DoBench(benchmark::State &state, size_t str_len, size_t n_delims) {
    const std::string s = MakeStringSprinkledWithDelims(str_len, n_delims);

    while (state.KeepRunning()) {
        std::vector<std::string> toks;
        benchmark::NoOptimize(Split(toks, s, " \f\n\r\t\v"));
    }
}

void String_Split_5_0(benchmark::State &state) { DoBench(state, 5, 0); }
void String_Split_5_1(benchmark::State &state) { DoBench(state, 5, 1); }
void String_Split_5_3(benchmark::State &state) { DoBench(state, 5, 3); }
void String_Split_5_4(benchmark::State &state) { DoBench(state, 5, 4); }

void String_Split_15_0(benchmark::State &state) { DoBench(state, 15, 0); }
void String_Split_15_1(benchmark::State &state) { DoBench(state, 15, 1); }
void String_Split_15_2(benchmark::State &state) { DoBench(state, 15, 2); }
void String_Split_15_5(benchmark::State &state) { DoBench(state, 15, 5); }

void String_Split_30_0(benchmark::State &state) { DoBench(state, 30, 0); }
void String_Split_30_1(benchmark::State &state) { DoBench(state, 30, 1); }
void String_Split_30_2(benchmark::State &state) { DoBench(state, 30, 2); }
void String_Split_30_5(benchmark::State &state) { DoBench(state, 30, 5); }

void String_Split_300_0(benchmark::State &state) { DoBench(state, 300, 0); }
void String_Split_300_1(benchmark::State &state) { DoBench(state, 300, 1); }
void String_Split_300_5(benchmark::State &state) { DoBench(state, 300, 5); }
void String_Split_300_10(benchmark::State &state) { DoBench(state, 300, 10); }
void String_Split_300_20(benchmark::State &state) { DoBench(state, 300, 20); }

void String_Split_1000_0(benchmark::State &state) { DoBench(state, 1000, 0); }
void String_Split_1000_5(benchmark::State &state) { DoBench(state, 1000, 5); }
void String_Split_1000_100(benchmark::State &state) { DoBench(state, 1000, 100); }
} // namespace

BENCHMARK(String_Split_5_0, 1200000);
BENCHMARK(String_Split_5_1, 1200000);
BENCHMARK(String_Split_5_3, 1200000);
BENCHMARK(String_Split_5_4, 1200000);

BENCHMARK(String_Split_15_0, 1200000);
BENCHMARK(String_Split_15_1, 1200000);
BENCHMARK(String_Split_15_2, 1200000);
BENCHMARK(String_Split_15_5, 1200000);

BENCHMARK(String_Split_30_0, 1200000);
BENCHMARK(String_Split_30_1, 1200000);
BENCHMARK(String_Split_30_2, 1200000);
BENCHMARK(String_Split_30_5, 1200000);

BENCHMARK(String_Split_300_0, 400000);
BENCHMARK(String_Split_300_1, 400000);
BENCHMARK(String_Split_300_5, 400000);
BENCHMARK(String_Split_300_10, 400000);
BENCHMARK(String_Split_300_20, 400000);

BENCHMARK(String_Split_1000_0, 200000);
BENCHMARK(String_Split_1000_5, 200000);
BENCHMARK(String_Split_1000_100, 200000);
