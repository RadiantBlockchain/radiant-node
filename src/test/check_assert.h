// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_TEST_CHECK_ASSERT_H
#define BITCOIN_TEST_CHECK_ASSERT_H

#include <boost/test/unit_test.hpp>

#include <functional>
#include <string_view>

/** Return value for CheckAssert() below */
enum class CheckAssertResult {
    Unsupported, NoAssertEncountered, AssertEncounteredWrongMessage, AssertEncountered
};

/**
 * Checks if a lambda results in an assert() being raised.
 * Note: This is accomplished by use of fork(). Unix systems only.
 * @pre func() must not modify the filesystem or database if called (such as writing blocks to disk).
 *      If it does do so, the behavior of this function is undefined.
 * @return One of the CheckAssertResult values above. If called on non-Unix, will return
 *         CheckAssertResult::Unsupported.
 * @exception std::runtime_error on low-level system error (cannot fork(), cannot pipe(), etc)
 */
[[nodiscard]]
CheckAssertResult CheckAssert(std::function<void()> func, std::string_view expectMessage = "");

/** Checks if an expression results in an assert() being raised. */
#define BCHN_CHECK_ASSERT(stmt, expectMessage) \
    do { \
        const auto res = CheckAssert([&]{ stmt; }, expectMessage); \
        if (res == CheckAssertResult::Unsupported) { \
            BOOST_WARN_MESSAGE(false, \
                               "Unsupported platform for assert() check: \"" BOOST_STRINGIZE(stmt) "\""); \
            break; \
        } \
        BOOST_CHECK_MESSAGE(res == CheckAssertResult::AssertEncountered, \
                            "Failed to trap the appropriate assert for: \"" BOOST_STRINGIZE(stmt) "\""); \
    } while (0)

#endif // BITCOIN_TEST_CHECK_ASSERT_H
