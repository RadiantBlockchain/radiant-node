// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/check_assert.h>
#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cassert>

BOOST_FIXTURE_TEST_SUITE(testlib_tests, BasicTestingSetup)

// Test the test suite facility for trapping assert() failures (see test/check_assert.h and test/check_assert.cpp)
BOOST_AUTO_TEST_CASE(check_assert_test) {
    if (CheckAssert([]{assert(false);}) == CheckAssertResult::Unsupported) {
        BOOST_WARN_MESSAGE(false, "Unable to test CheckAssert, not supported on this platform; skipping.");
        return; // skip this test for unsupported platforms
    }
    BOOST_CHECK(CheckAssert([]{ assert(!"any message"); })
                == CheckAssertResult::AssertEncountered);
    BOOST_CHECK(CheckAssert([]{ assert(!"wrong message"); }, "right message")
                == CheckAssertResult::AssertEncounteredWrongMessage);
    BOOST_CHECK(CheckAssert([]{ assert(!"right message"); }, "right message")
                == CheckAssertResult::AssertEncountered);
    BOOST_CHECK(CheckAssert([]{}, "ignored message")
                == CheckAssertResult::NoAssertEncountered);
    BOOST_CHECK(CheckAssert([]{ BOOST_CHECK(false); })
                == CheckAssertResult::NoAssertEncountered);

    // check that the macro works
    BCHN_CHECK_ASSERT(assert(!"expected message"), "expected message");
}

BOOST_AUTO_TEST_SUITE_END()
