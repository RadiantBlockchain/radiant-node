// Copyright (c) 2015-2016 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

// Simple micro-benchmarking framework; API mostly matches a subset of the
// Google Benchmark framework (see https://github.com/google/benchmark)
// Why not use the Google Benchmark framework? Because adding Yet Another
// Dependency (that uses cmake as its build system and has lots of features we
// don't need) isn't worth it.

/*
 * Usage:

static void CODE_TO_TIME(benchmark::State& state)
{
    ... do any setup needed...
    while (state.KeepRunning()) {
       ... do stuff you want to time...
    }
    ... do any cleanup needed...
}

// default to running benchmark for 5000 iterations
BENCHMARK(CODE_TO_TIME, 5000);

 */

namespace benchmark {
// In case high_resolution_clock is steady, prefer that, otherwise use
// steady_clock.
struct best_clock {
    using hi_res_clock = std::chrono::high_resolution_clock;
    using steady_clock = std::chrono::steady_clock;
    using type = std::conditional<hi_res_clock::is_steady, hi_res_clock,
                                  steady_clock>::type;
};
using clock = best_clock::type;
using time_point = clock::time_point;
using duration = clock::duration;

class Printer;

class State {
public:
    std::string m_name;
    uint64_t m_num_iters_left;
    const uint64_t m_num_iters;
    const uint64_t m_num_evals;
    std::vector<double> m_elapsed_results;
    time_point m_start_time;

    bool UpdateTimer(time_point finish_time);

    State(const std::string &name, uint64_t num_evals, double num_iters,
          Printer &printer)
        : m_name(name), m_num_iters_left(0), m_num_iters(num_iters),
          m_num_evals(num_evals) {}

    inline bool KeepRunning() {
        if (m_num_iters_left--) {
            return true;
        }

        bool result = UpdateTimer(clock::now());
        // measure again so runtime of UpdateTimer is not included
        m_start_time = clock::now();
        return result;
    }
};

typedef std::function<void(State &)> BenchFunction;

class BenchRunner {
    struct Bench {
        BenchFunction func;
        uint64_t num_iters_for_one_second;
    };
    typedef std::map<std::string, Bench> BenchmarkMap;
    static BenchmarkMap &benchmarks();

public:
    BenchRunner(const std::string &name, BenchFunction func,
                uint64_t num_iters_for_one_second);

    static void RunAll(Printer &printer, uint64_t num_evals, double scaling,
                       const std::string &filter, bool is_list_only);
};

// interface to output benchmark results.
class Printer {
public:
    virtual ~Printer() {}
    virtual void header() = 0;
    virtual void result(const State &state) = 0;
    virtual void footer() = 0;
};

// default printer to console, shows min, max, median.
class ConsolePrinter : public Printer {
public:
    void header() override;
    void result(const State &state) override;
    void footer() override;
};

// creates box plot with plotly.js
class PlotlyPrinter : public Printer {
public:
    PlotlyPrinter(const std::string &plotly_url, int64_t width, int64_t height);
    void header() override;
    void result(const State &state) override;
    void footer() override;

private:
    std::string m_plotly_url;
    int64_t m_width;
    int64_t m_height;
};


namespace internal {
/// Internal function (called with pointers only by NoOptimize() below). This function is a no-op.
extern void
#if defined(__clang__)
__attribute__((optnone)) /* disable optimizations -- should keep this function also from being reordered */
#elif defined(__GNUC__)
__attribute__((optimize(0),no_reorder))
#endif
NoOptimize(...) noexcept;
} // namespace internal

/// This is a "do nothing" function that can take any number of arguments. The intent here is to
/// hopefully not have the optimizer elide some calls during a benchmark iteration. Use this to
/// wrap function calls or to denote objects during a benchmark iteration which you would like
/// the optimizer to not elide or reorder.
template <typename...Args>
constexpr void NoOptimize(Args && ...args) noexcept {
    auto GetPointer = [](auto && arg) noexcept {
        if constexpr (std::is_pointer_v<std::remove_reference_t<decltype(arg)>>)
            return arg;
        else
            return &arg;
    };
    internal::NoOptimize(GetPointer(std::forward<Args>(args))...);
}

} // namespace benchmark

// BENCHMARK(foo, num_iters_for_one_second) expands to:  benchmark::BenchRunner
// bench_11foo("foo", num_iterations);
// Choose a num_iters_for_one_second that takes roughly 1 second. The goal is
// that all benchmarks should take approximately
// the same time, and scaling factor can be used that the total time is
// appropriate for your system.

#define CAT(a, b) CAT_I(a, b)
#define CAT_I(a, b) a ## b
#define STRINGIZE_TEXT(...) #__VA_ARGS__

#define BENCHMARK(n, num_iters_for_one_second)                                 \
    benchmark::BenchRunner CAT(bench_, CAT(__LINE__, n))(                      \
        STRINGIZE_TEXT(n), n, (num_iters_for_one_second));
