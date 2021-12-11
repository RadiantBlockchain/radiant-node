// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <type_traits>

void UninterruptibleSleep(const std::chrono::microseconds &n);

/**
 * DEPRECATED
 * Use either GetSystemTimeInSeconds (not mockable) or GetTime<T> (mockable)
 */
int64_t GetTime();

/** Returns the system time (not mockable) */
int64_t GetTimeMillis();
/** Returns the system time (not mockable) */
int64_t GetTimeMicros();
/** Returns the system time (not mockable) */
// Like GetTime(), but not mockable
int64_t GetSystemTimeInSeconds();

/**
 * Returns a monotonic high resolution timestamp, in nanoseconds, suitable for
 * profiling code.
 *
 * Note that this timestamp is monotonic and *not* at all related to the system
 * clock. Its resolution is at least 1 microsecond (or better!). This value
 * increases even when the system is asleep. Not mockable.
*/
int64_t GetPerfTimeNanos();

/** For testing. Set e.g. with the setmocktime rpc, or -mocktime argument */
void SetMockTime(int64_t nMockTimeIn);
/** For testing */
int64_t GetMockTime();

void MilliSleep(int64_t n);

/** Return system time (or mocked time, if set) */
template <typename T> T GetTime();

/**
 * ISO 8601 formatting is preferred. Use the FormatISO8601{DateTime,Date}
 * helper functions if possible.
 */
std::string FormatISO8601DateTime(int64_t nTime);
std::string FormatISO8601Date(int64_t nTime);
int64_t ParseISO8601DateTime(const std::string &str);

/// Tic, or "time-code". A convenience class that can be used to profile sections of code.
///
/// Takes a timestamp (via GetPerfTimeNanos) when it is constructed. Provides various utility
/// methods to read back the elapsed time since construction, in various units.
class Tic {
    static int64_t now() { return GetPerfTimeNanos(); }
    int64_t saved = now();
    template <typename T>
    using T_if_is_arithmetic = std::enable_if_t<std::is_arithmetic_v<T>, T>; // SFINAE evaluates to T if T is an arithmetic type
    template <typename T>
    T_if_is_arithmetic<T> elapsed(T factor) const {
        if (saved >= 0) {
            // non-frozen timestamp; `saved` is the timestamp at construction
            return T((now() - saved)) / factor;
        } else {
            // frozen timestamp; `saved` is the negative of the elapsed time when fin() was called
            return T(std::abs(saved)) / factor;
        }
    }
    std::string format(double val, int precision) const;
    std::string format(int64_t nsec) const;

public:
    /// Returns the number of seconds elapsed since construction (note the default return type here is double)
    template <typename T = double>
    T_if_is_arithmetic<T>
    /* T */ secs() const { return elapsed(T(1e9)); }

    /// Returns the number of seconds elapsed since construction formatted as a floating point string (for logging)
    std::string secsStr(int precision = 3) const { return format(secs(), precision); }

    /// " milliseconds (note the default return type here is int64_t)
    template <typename T = int64_t>
    T_if_is_arithmetic<T>
    /* T */ msec() const { return elapsed(T(1e6)); }

    /// Returns the number of milliseconds formatted as a floating point string, useful for logging
    std::string msecStr(int precision = 3) const { return format(msec<double>(), precision); }

    /// " microseconds (note the default return type here is int64_t)
    template <typename T = int64_t>
    T_if_is_arithmetic<T>
    /* T */ usec() const { return elapsed(T(1e3)); }

    /// Returns the number of microseconds formatted as a floating point string, useful for logging
    std::string usecStr(int precision = 3) const { return format(usec<double>(), precision); }

    /// " nanoseconds
    int64_t nsec() const { return elapsed(int64_t(1)); }

    /// Returns the number of nanoseconds formatted as an integer string, useful for logging
    std::string nsecStr() const { return format(nsec()); }

    /// Save the current time. After calling this, secs(), msec(), usec(), and nsec() above will "freeze"
    /// and they will forever always return the time from construction until fin() was called.
    ///
    /// Subsequent calls to fin() are no-ops.  Once fin()'d a Tic cannot be set to continue counting time.
    /// To restart the timer, assign it a default constructed value and it will begin counting again from
    /// that point forward e.g.:  mytic = Tic()
    void fin() { saved = -nsec(); }
};
