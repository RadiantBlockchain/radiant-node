// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <amount.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>

BOOST_FIXTURE_TEST_SUITE(amount_tests, BasicTestingSetup)

static void CheckAmounts(int64_t aval, int64_t bval) {
    Amount a(aval * SATOSHI), b(bval * SATOSHI);

    // Equality
    BOOST_CHECK_EQUAL(a == b, aval == bval);
    BOOST_CHECK_EQUAL(b == a, aval == bval);

    BOOST_CHECK_EQUAL(a != b, aval != bval);
    BOOST_CHECK_EQUAL(b != a, aval != bval);

    // Comparison
    BOOST_CHECK_EQUAL(a < b, aval < bval);
    BOOST_CHECK_EQUAL(b < a, bval < aval);

    BOOST_CHECK_EQUAL(a > b, aval > bval);
    BOOST_CHECK_EQUAL(b > a, bval > aval);

    BOOST_CHECK_EQUAL(a <= b, aval <= bval);
    BOOST_CHECK_EQUAL(b <= a, bval <= aval);

    BOOST_CHECK_EQUAL(a >= b, aval >= bval);
    BOOST_CHECK_EQUAL(b >= a, bval >= aval);

    // Unary minus
    BOOST_CHECK_EQUAL(-a, -aval * SATOSHI);
    BOOST_CHECK_EQUAL(-b, -bval * SATOSHI);

    // Addition and subtraction.
    BOOST_CHECK_EQUAL(a + b, b + a);
    BOOST_CHECK_EQUAL(a + b, (aval + bval) * SATOSHI);

    BOOST_CHECK_EQUAL(a - b, -(b - a));
    BOOST_CHECK_EQUAL(a - b, (aval - bval) * SATOSHI);

    // Multiplication
    BOOST_CHECK_EQUAL(aval * b, bval * a);
    BOOST_CHECK_EQUAL(aval * b, (aval * bval) * SATOSHI);

    // Division
    if (b != Amount::zero()) {
        BOOST_CHECK_EQUAL(a / b, aval / bval);
        BOOST_CHECK_EQUAL(a / bval, (a / b) * SATOSHI);
    }

    if (a != Amount::zero()) {
        BOOST_CHECK_EQUAL(b / a, bval / aval);
        BOOST_CHECK_EQUAL(b / aval, (b / a) * SATOSHI);
    }

    // Modulus
    if (b != Amount::zero()) {
        BOOST_CHECK_EQUAL(a % b, a % bval);
        BOOST_CHECK_EQUAL(a % b, (aval % bval) * SATOSHI);
    }

    if (a != Amount::zero()) {
        BOOST_CHECK_EQUAL(b % a, b % aval);
        BOOST_CHECK_EQUAL(b % a, (bval % aval) * SATOSHI);
    }

    // OpAssign
    Amount v;
    BOOST_CHECK_EQUAL(v, Amount::zero());
    v += a;
    BOOST_CHECK_EQUAL(v, a);
    v += b;
    BOOST_CHECK_EQUAL(v, a + b);
    v += b;
    BOOST_CHECK_EQUAL(v, a + 2 * b);
    v -= 2 * a;
    BOOST_CHECK_EQUAL(v, 2 * b - a);
}

BOOST_AUTO_TEST_CASE(AmountTests) {
    std::array<int64_t, 8> values = {{-23, -1, 0, 1, 2, 3, 42, 99999999}};

    for (int64_t i : values) {
        for (int64_t j : values) {
            CheckAmounts(i, j);
        }
    }

    BOOST_CHECK_EQUAL(COIN + COIN, 2 * COIN);
    BOOST_CHECK_EQUAL(2 * COIN + COIN, 3 * COIN);
    BOOST_CHECK_EQUAL(-1 * COIN + COIN, Amount::zero());

    BOOST_CHECK_EQUAL(COIN - COIN, Amount::zero());
    BOOST_CHECK_EQUAL(COIN - 2 * COIN, -1 * COIN);
}

BOOST_AUTO_TEST_CASE(MoneyRangeTest) {
    BOOST_CHECK_EQUAL(MoneyRange(-SATOSHI), false);
    BOOST_CHECK_EQUAL(MoneyRange(Amount::zero()), true);
    BOOST_CHECK_EQUAL(MoneyRange(SATOSHI), true);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY), true);
    BOOST_CHECK_EQUAL(MoneyRange(MAX_MONEY + SATOSHI), false);
}

BOOST_AUTO_TEST_CASE(BinaryOperatorTest) {
    CFeeRate a, b;
    a = CFeeRate(1 * SATOSHI);
    b = CFeeRate(2 * SATOSHI);
    BOOST_CHECK(a < b);
    BOOST_CHECK(b > a);
    BOOST_CHECK(a == a);
    BOOST_CHECK(a <= b);
    BOOST_CHECK(a <= a);
    BOOST_CHECK(b >= a);
    BOOST_CHECK(b >= b);
    // a should be 0.00000002 BTC/kB now
    a += a;
    BOOST_CHECK(a == b);
}

BOOST_AUTO_TEST_CASE(ToStringTest) {
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(false), "1.23456000");
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(true), "1.23456");
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(false, false), "1.23456000");
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(false, true), (123'456'000 * SATOSHI).ToString(false, false)); // 2nd arg shouldn't change behaviour if 1st is false
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(true, false), "1.23456");
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(true, true), "1.23456");
    BOOST_CHECK_EQUAL((123'456'000 * SATOSHI).ToString(), "1.23456"); // check default behaviour

    BOOST_CHECK_EQUAL((1230 * COIN).ToString(false), "1230.00000000");
    BOOST_CHECK_EQUAL((1230 * COIN).ToString(true), "1230.0");
    BOOST_CHECK_EQUAL((1230 * COIN).ToString(false, false), "1230.00000000");
    BOOST_CHECK_EQUAL((1230 * COIN).ToString(false, true), (1230 * COIN).ToString(false, false));
    BOOST_CHECK_EQUAL((1230 * COIN).ToString(true, false), "1230.0");
    BOOST_CHECK_EQUAL((1230 * COIN).ToString(true, true), "1230");
    BOOST_CHECK_EQUAL((1230 * COIN).ToString(), "1230.0");

    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(false), "-1.23456000");
    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(true), "-1.23456");
    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(false, false), "-1.23456000");
    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(false, true), (-123'456'000 * SATOSHI).ToString(false, false));
    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(true, false), "-1.23456");
    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(true, true), "-1.23456");
    BOOST_CHECK_EQUAL((-123'456'000 * SATOSHI).ToString(), "-1.23456");

    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(false), "-1230.00000000");
    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(true), "-1230.0");
    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(false, false), "-1230.00000000");
    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(false, true), (-1230 * COIN).ToString(false, false));
    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(true, false), "-1230.0");
    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(true, true), "-1230");
    BOOST_CHECK_EQUAL((-1230 * COIN).ToString(), "-1230.0");

    BOOST_CHECK_EQUAL(COIN.ToString(false), "1.00000000");
    BOOST_CHECK_EQUAL(COIN.ToString(true), "1.0");
    BOOST_CHECK_EQUAL(COIN.ToString(false, false), "1.00000000");
    BOOST_CHECK_EQUAL(COIN.ToString(false, true), COIN.ToString(false, false));
    BOOST_CHECK_EQUAL(COIN.ToString(true, false), "1.0");
    BOOST_CHECK_EQUAL(COIN.ToString(true, true), "1");
    BOOST_CHECK_EQUAL(COIN.ToString(), "1.0");

    BOOST_CHECK_EQUAL((-COIN).ToString(false), "-1.00000000");
    BOOST_CHECK_EQUAL((-COIN).ToString(true), "-1.0");
    BOOST_CHECK_EQUAL((-COIN).ToString(false, false), "-1.00000000");
    BOOST_CHECK_EQUAL((-COIN).ToString(false, true), (-COIN).ToString(false, false));
    BOOST_CHECK_EQUAL((-COIN).ToString(true, false), "-1.0");
    BOOST_CHECK_EQUAL((-COIN).ToString(true, true), "-1");
    BOOST_CHECK_EQUAL((-COIN).ToString(), "-1.0");

    BOOST_CHECK_EQUAL((100 * COIN).ToString(false), "100.00000000");
    BOOST_CHECK_EQUAL((100 * COIN).ToString(true), "100.0");
    BOOST_CHECK_EQUAL((100 * COIN).ToString(false, false), "100.00000000");
    BOOST_CHECK_EQUAL((100 * COIN).ToString(false, true), (100 * COIN).ToString(false, false));
    BOOST_CHECK_EQUAL((100 * COIN).ToString(true, false), "100.0");
    BOOST_CHECK_EQUAL((100 * COIN).ToString(true, true), "100");
    BOOST_CHECK_EQUAL((100 * COIN).ToString(), "100.0");

    BOOST_CHECK_EQUAL((-100 * COIN).ToString(false), "-100.00000000");
    BOOST_CHECK_EQUAL((-100 * COIN).ToString(true), "-100.0");
    BOOST_CHECK_EQUAL((-100 * COIN).ToString(false, false), "-100.00000000");
    BOOST_CHECK_EQUAL((-100 * COIN).ToString(false, true), (-100 * COIN).ToString(false, false));
    BOOST_CHECK_EQUAL((-100 * COIN).ToString(true, false), "-100.0");
    BOOST_CHECK_EQUAL((-100 * COIN).ToString(true, true), "-100");
    BOOST_CHECK_EQUAL((-100 * COIN).ToString(), "-100.0");

    BOOST_CHECK_EQUAL(SATOSHI.ToString(false), "0.00000001");
    BOOST_CHECK_EQUAL(SATOSHI.ToString(true), "0.00000001");
    BOOST_CHECK_EQUAL(SATOSHI.ToString(false, false), "0.00000001");
    BOOST_CHECK_EQUAL(SATOSHI.ToString(false, true), SATOSHI.ToString(false, false));
    BOOST_CHECK_EQUAL(SATOSHI.ToString(true, false), "0.00000001");
    BOOST_CHECK_EQUAL(SATOSHI.ToString(true, true), "0.00000001");
    BOOST_CHECK_EQUAL(SATOSHI.ToString(), "0.00000001");

    BOOST_CHECK_EQUAL((-SATOSHI).ToString(false), "-0.00000001");
    BOOST_CHECK_EQUAL((-SATOSHI).ToString(true), "-0.00000001");
    BOOST_CHECK_EQUAL((-SATOSHI).ToString(false, false), "-0.00000001");
    BOOST_CHECK_EQUAL((-SATOSHI).ToString(false, true), (-SATOSHI).ToString(false, false));
    BOOST_CHECK_EQUAL((-SATOSHI).ToString(true, false), "-0.00000001");
    BOOST_CHECK_EQUAL((-SATOSHI).ToString(true, true), "-0.00000001");
    BOOST_CHECK_EQUAL((-SATOSHI).ToString(), "-0.00000001");

    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(false), "0.00000100");
    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(true), "0.000001");
    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(false, false), "0.00000100");
    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(false, true), (100 * SATOSHI).ToString(false, false));
    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(true, false), "0.000001");
    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(true, true), "0.000001");
    BOOST_CHECK_EQUAL((100 * SATOSHI).ToString(), "0.000001");

    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(false), "-0.00000100");
    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(true), "-0.000001");
    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(false, false), "-0.00000100");
    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(false, true), (-100 * SATOSHI).ToString(false, false));
    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(true, false), "-0.000001");
    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(true, true), "-0.000001");
    BOOST_CHECK_EQUAL((-100 * SATOSHI).ToString(), "-0.000001");

    BOOST_CHECK_EQUAL((COIN / 10).ToString(false), "0.10000000");
    BOOST_CHECK_EQUAL((COIN / 10).ToString(true), "0.1");
    BOOST_CHECK_EQUAL((COIN / 10).ToString(false, false), "0.10000000");
    BOOST_CHECK_EQUAL((COIN / 10).ToString(false, true), (COIN / 10).ToString(false, false));
    BOOST_CHECK_EQUAL((COIN / 10).ToString(true, false), "0.1");
    BOOST_CHECK_EQUAL((COIN / 10).ToString(true, true), "0.1");
    BOOST_CHECK_EQUAL((COIN / 10).ToString(), "0.1");

    BOOST_CHECK_EQUAL((-COIN / 10).ToString(false), "-0.10000000");
    BOOST_CHECK_EQUAL((-COIN / 10).ToString(true), "-0.1");
    BOOST_CHECK_EQUAL((-COIN / 10).ToString(false, false), "-0.10000000");
    BOOST_CHECK_EQUAL((-COIN / 10).ToString(false, true), (-COIN / 10).ToString(false, false));
    BOOST_CHECK_EQUAL((-COIN / 10).ToString(true, false), "-0.1");
    BOOST_CHECK_EQUAL((-COIN / 10).ToString(true, true), "-0.1");
    BOOST_CHECK_EQUAL((-COIN / 10).ToString(), "-0.1");


    BOOST_CHECK_EQUAL(Amount{}.ToString(false), "0.00000000");
    BOOST_CHECK_EQUAL(Amount{}.ToString(true), "0.0");
    BOOST_CHECK_EQUAL(Amount{}.ToString(false, false), "0.00000000");
    BOOST_CHECK_EQUAL(Amount{}.ToString(false, true), Amount{}.ToString(false, false));
    BOOST_CHECK_EQUAL(Amount{}.ToString(true, false), "0.0");
    BOOST_CHECK_EQUAL(Amount{}.ToString(true, true), "0");
    BOOST_CHECK_EQUAL(Amount{}.ToString(), "0.0");

    BOOST_CHECK_EQUAL((INT64_MAX * SATOSHI).ToString(), "92233720368.54775807");
    BOOST_CHECK_EQUAL((-INT64_MAX * SATOSHI).ToString(), "-92233720368.54775807");
    BOOST_CHECK_EQUAL((INT64_MIN * SATOSHI).ToString(), "-92233720368.54775808");
}

BOOST_AUTO_TEST_SUITE_END()
