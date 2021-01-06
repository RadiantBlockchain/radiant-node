// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#if defined(HAVE_CONFIG_H)
#include <config/bitcoin-config.h>
#endif

#include <util/time.h>

#include <boost/date_time/posix_time/posix_time.hpp>
#include <boost/thread.hpp>

#ifdef _WIN32
// The below are only used by GetPerfTimeNanos() in the Windows case
#include <algorithm>
#include <profileapi.h>
#endif

#include <tinyformat.h>

#include <atomic>
#include <ctime>
#include <thread>

void UninterruptibleSleep(const std::chrono::microseconds &n) {
    std::this_thread::sleep_for(n);
}

//! For unit testing
static std::atomic<int64_t> nMockTime(0);

int64_t GetTime() {
    int64_t mocktime = nMockTime.load(std::memory_order_relaxed);
    if (mocktime) {
        return mocktime;
    }

    time_t now = time(nullptr);
    assert(now > 0);
    return now;
}

template <typename T> T GetTime() {
    const std::chrono::seconds mocktime{
        nMockTime.load(std::memory_order_relaxed)};

    return std::chrono::duration_cast<T>(
        mocktime.count() ? mocktime
                         : std::chrono::microseconds{GetTimeMicros()});
}
template std::chrono::seconds GetTime();
template std::chrono::milliseconds GetTime();
template std::chrono::microseconds GetTime();

void SetMockTime(int64_t nMockTimeIn) {
    assert(nMockTimeIn >= 0);
    nMockTime.store(nMockTimeIn, std::memory_order_relaxed);
}

int64_t GetMockTime() {
    return nMockTime.load(std::memory_order_relaxed);
}

int64_t GetTimeMillis() {
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1)))
                      .total_milliseconds();
    assert(now > 0);
    return now;
}

int64_t GetTimeMicros() {
    int64_t now = (boost::posix_time::microsec_clock::universal_time() -
                   boost::posix_time::ptime(boost::gregorian::date(1970, 1, 1)))
                      .total_microseconds();
    assert(now > 0);
    return now;
}

int64_t GetPerfTimeNanos() {
#ifdef _WIN32
    // Windows lacks a decent high resolution clock source on some C++ implementations (such as MinGW). So we
    // query the OS's QPC mechanism, which, on Windows 7+ is very fast to query and guaranteed to be accurate,
    // with sub-microsecond precision. It is also monotonic ("steady").
    static const auto queryPerfCtr = []() -> int64_t {
        // This is is guaranteed to be initialized once atomically the first time through this function.
        static const int64_t freq = []{
            // Read the performance counter frequency in counts per second.
            LARGE_INTEGER val;
            QueryPerformanceFrequency(&val);
            return int64_t(val.QuadPart);
        }();
        LARGE_INTEGER ct;
        QueryPerformanceCounter(&ct);   // reads the performance counter (in ticks)
        // return ticks converted to nanoseconds
        return (int64_t(ct.QuadPart) * int64_t(1'000'000'000)) / std::max(freq, int64_t(1));
    };
    // Save the timestamp of the first time through here to start with low values after app init.
    static const int64_t t0 = queryPerfCtr();
    return queryPerfCtr() - t0;
#else
    // Otherwise, get the best clock we can from the implementation, prefering high resolution clocks
    // to the 'steady_clock' (but only if it is steady). This branch is taken on Linux, Darwin or on BSD,
    // all of which have very high quality implementations of the high_resolution_clock.
    using Clock = std::conditional_t<std::chrono::high_resolution_clock::is_steady,
                                     std::chrono::high_resolution_clock,
                                     std::chrono::steady_clock>;
    // Ensure compiler and C++ library provide the microsecond or better precision we need. If not, fall-back to
    // good old GetTimeMicros().
    if constexpr (std::ratio_less_equal_v<Clock::period, std::micro>) {
        // Save the timestamp of the first time through here to start with low values after app init.
        static const auto t0 = Clock::now();
        return std::chrono::duration<int64_t, std::nano>(Clock::now() - t0).count();
    } else {
        // This branch really should never be taken on the major platforms we support and is normally compiled-out.
        return GetTimeMicros() * 1'000;
    }
#endif
}

int64_t GetSystemTimeInSeconds() {
    return GetTimeMicros() / 1000000;
}

void MilliSleep(int64_t n) {
    boost::this_thread::sleep_for(boost::chrono::milliseconds(n));
}

std::string FormatISO8601DateTime(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _WIN32
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%04i-%02i-%02iT%02i:%02i:%02iZ", ts.tm_year + 1900,
                     ts.tm_mon + 1, ts.tm_mday, ts.tm_hour, ts.tm_min,
                     ts.tm_sec);
}

std::string FormatISO8601Date(int64_t nTime) {
    struct tm ts;
    time_t time_val = nTime;
#ifdef _WIN32
    gmtime_s(&ts, &time_val);
#else
    gmtime_r(&time_val, &ts);
#endif
    return strprintf("%04i-%02i-%02i", ts.tm_year + 1900, ts.tm_mon + 1,
                     ts.tm_mday);
}

int64_t ParseISO8601DateTime(const std::string &str) {
    static const boost::posix_time::ptime epoch =
        boost::posix_time::from_time_t(0);
    static const std::locale loc(
        std::locale::classic(),
        new boost::posix_time::time_input_facet("%Y-%m-%dT%H:%M:%SZ"));
    std::istringstream iss(str);
    iss.imbue(loc);
    boost::posix_time::ptime ptime(boost::date_time::not_a_date_time);
    iss >> ptime;
    if (ptime.is_not_a_date_time() || epoch > ptime) {
        return 0;
    }
    return (ptime - epoch).total_seconds();
}


std::string Tic::format(double val, int precision) const { return strprintf("%1.*f", precision, val); }
std::string Tic::format(int64_t nsec) const { return strprintf("%i", nsec); }
