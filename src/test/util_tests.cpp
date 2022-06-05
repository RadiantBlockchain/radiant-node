// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/system.h>

#include <clientversion.h>
#include <primitives/transaction.h>
#include <sync.h>
#include <tinyformat.h>
#include <util/bit_cast.h>
#include <util/defer.h>
#include <util/moneystr.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/vector.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#ifndef WIN32
#include <csignal>
#include <sys/types.h>
#include <sys/wait.h>
#endif

BOOST_FIXTURE_TEST_SUITE(util_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(util_criticalsection) {
    RecursiveMutex cs;

    do {
        LOCK(cs);
        break;

        BOOST_ERROR("break was swallowed!");
    } while (0);

    do {
        TRY_LOCK(cs, lockTest);
        if (lockTest) {
            // Needed to suppress "Test case [...] did not check any assertions"
            BOOST_CHECK(true);
            break;
        }

        BOOST_ERROR("break was swallowed!");
    } while (0);
}

static const uint8_t ParseHex_expected[65] = {
    0x04, 0x67, 0x8a, 0xfd, 0xb0, 0xfe, 0x55, 0x48, 0x27, 0x19, 0x67,
    0xf1, 0xa6, 0x71, 0x30, 0xb7, 0x10, 0x5c, 0xd6, 0xa8, 0x28, 0xe0,
    0x39, 0x09, 0xa6, 0x79, 0x62, 0xe0, 0xea, 0x1f, 0x61, 0xde, 0xb6,
    0x49, 0xf6, 0xbc, 0x3f, 0x4c, 0xef, 0x38, 0xc4, 0xf3, 0x55, 0x04,
    0xe5, 0x1e, 0xc1, 0x12, 0xde, 0x5c, 0x38, 0x4d, 0xf7, 0xba, 0x0b,
    0x8d, 0x57, 0x8a, 0x4c, 0x70, 0x2b, 0x6b, 0xf1, 0x1d, 0x5f};
BOOST_AUTO_TEST_CASE(util_ParseHex) {
    std::vector<uint8_t> result;
    std::vector<uint8_t> expected(
        ParseHex_expected, ParseHex_expected + sizeof(ParseHex_expected));
    // Basic test vector
    result = ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0"
                      "ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d"
                      "578a4c702b6bf11d5f");
    BOOST_CHECK_EQUAL_COLLECTIONS(result.begin(), result.end(),
                                  expected.begin(), expected.end());

    // Spaces between bytes must be supported
    result = ParseHex("12 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x12 && result[1] == 0x34 &&
                result[2] == 0x56 && result[3] == 0x78);

    // Leading space must be supported (used in BerkeleyEnvironment::Salvage)
    result = ParseHex(" 89 34 56 78");
    BOOST_CHECK(result.size() == 4 && result[0] == 0x89 && result[1] == 0x34 &&
                result[2] == 0x56 && result[3] == 0x78);

    // Stop parsing at invalid value
    result = ParseHex("1234 invalid 1234");
    BOOST_CHECK(result.size() == 2 && result[0] == 0x12 && result[1] == 0x34);
}

BOOST_AUTO_TEST_CASE(util_HexStr) {
    BOOST_CHECK_EQUAL(HexStr(ParseHex_expected),
                      "04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0"
                      "ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d"
                      "578a4c702b6bf11d5f");

    BOOST_CHECK_EQUAL(HexStr(Span<const uint8_t>(ParseHex_expected).first(5), true), "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(HexStr(Span<const uint8_t>(ParseHex_expected).last(0)), "");

    BOOST_CHECK_EQUAL(HexStr(Span<const uint8_t>(ParseHex_expected).last(0), true), "");

    BOOST_CHECK_EQUAL(HexStr(Span<const uint8_t>(ParseHex_expected).first(0)), "");

    BOOST_CHECK_EQUAL(HexStr(Span<const uint8_t>(ParseHex_expected).first(0), true), "");

    std::vector<uint8_t> ParseHex_vec(ParseHex_expected, ParseHex_expected + 5);

    BOOST_CHECK_EQUAL(HexStr(ParseHex_vec, true), "04 67 8a fd b0");

    BOOST_CHECK_EQUAL(HexStr(ParseHex_vec.rbegin(), ParseHex_vec.rend()),
                      "b0fd8a6704");

    BOOST_CHECK_EQUAL(HexStr(ParseHex_vec.rbegin(), ParseHex_vec.rend(), true),
                      "b0 fd 8a 67 04");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 1),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "04");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 1),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "04");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 5),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "b0fd8a6704");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 5),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected), true),
        "b0 fd 8a 67 04");

    BOOST_CHECK_EQUAL(
        HexStr(std::reverse_iterator<const uint8_t *>(ParseHex_expected + 65),
               std::reverse_iterator<const uint8_t *>(ParseHex_expected)),
        "5f1df16b2b704c8a578d0bbaf74d385cde12c11ee50455f3c438ef4c3fbcf649b6de61"
        "1feae06279a60939e028a8d65c10b73071a6f16719274855feb0fd8a6704");

    // check that if begin > end, empty string is returned
    BOOST_CHECK_EQUAL(HexStr(ParseHex_expected + 10, ParseHex_expected + 1, true), "");
}

/// Test string utility functions: trim
BOOST_AUTO_TEST_CASE(util_TrimString, *boost::unit_test::timeout(5)) {
    static const std::string pattern = " \t\r\n";
    BOOST_CHECK_EQUAL(TrimString(" \t asdf \t fdsa\r \n", pattern), std::string{"asdf \t fdsa"});
    BOOST_CHECK_EQUAL(TrimString("\t\t\t asdf \t fdsa\r\r\r ", pattern), std::string{"asdf \t fdsa"});
    BOOST_CHECK_EQUAL(TrimString("", pattern), std::string{""});
    BOOST_CHECK_EQUAL(TrimString("\t\t\t", pattern), std::string{""});
    BOOST_CHECK_EQUAL(TrimString("\t\t\tA", pattern), std::string{"A"});
    BOOST_CHECK_EQUAL(TrimString("A\t\t\tA", pattern), std::string{"A\t\t\tA"});
    BOOST_CHECK_EQUAL(TrimString("A\t\t\t", pattern), std::string{"A"});
    BOOST_CHECK_EQUAL(TrimString(" \f\n\r\t\vasdf fdsa \f\n\r\t\v"), std::string{"asdf fdsa"}); // test default parameters
}

/// Test string utility functions: join
BOOST_AUTO_TEST_CASE(util_Join, *boost::unit_test::timeout(5)) {
    // Normal version
    BOOST_CHECK_EQUAL(Join({}, ", "), "");
    BOOST_CHECK_EQUAL(Join({"foo"}, ", "), "foo");
    BOOST_CHECK_EQUAL(Join({"foo", "bar"}, ", "), "foo, bar");

    // Version with unary operator
    const auto op_upper = [](const std::string &s) { return ToUpper(s); };
    BOOST_CHECK_EQUAL(Join<std::string>({}, ", ", op_upper), "");
    BOOST_CHECK_EQUAL(Join<std::string>({"foo"}, ", ", op_upper), "FOO");
    BOOST_CHECK_EQUAL(Join<std::string>({"foo", "bar"}, ", ", op_upper), "FOO, BAR");
}

static void SplitWrapper(std::vector<std::string> &result, std::string_view str,
                         std::optional<std::string_view> delims = std::nullopt, bool tokenCompress = false) {
    std::set<std::string> set;

    if (delims) {
        Split(result, str, *delims, tokenCompress);
        Split(set, str, *delims, tokenCompress);
    } else {
        // this is so that this test doesn't have to keep track of whatever the default delim arg is for Split()
        Split(result, str);
        Split(set, str);
    }

    // check that using std::set produces correct results as compared to the std::vector version.
    BOOST_CHECK(set == std::set<std::string>(result.begin(), result.end()));
}

/// Test string utility functions: split
BOOST_AUTO_TEST_CASE(util_Split, *boost::unit_test::timeout(5)) {
    std::vector<std::string> result;

    SplitWrapper(result, "", " \n");
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK(result[0].empty());

    SplitWrapper(result, "   ", " ");
    BOOST_CHECK_EQUAL(result.size(), 4);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[3].empty());

    SplitWrapper(result, "  .", " .");
    BOOST_CHECK_EQUAL(result.size(), 4);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[3].empty());

    SplitWrapper(result, "word", " \n");
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK_EQUAL(result[0], "word");

    SplitWrapper(result, "simple\ntest", " .\n");
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK_EQUAL(result[0], "simple");
    BOOST_CHECK_EQUAL(result[1], "test");

    SplitWrapper(result, "This is a test.", " .");
    BOOST_CHECK_EQUAL(result.size(), 5);
    BOOST_CHECK_EQUAL(result[0], "This");
    BOOST_CHECK_EQUAL(result[3], "test");
    BOOST_CHECK(result[4].empty());

    SplitWrapper(result, "This is a test...", " .");
    BOOST_CHECK_EQUAL(result.size(), 7);
    BOOST_CHECK_EQUAL(result[0], "This");
    BOOST_CHECK_EQUAL(result[3], "test");
    BOOST_CHECK(result[4].empty());

    SplitWrapper(result, " \f\n\r\t\vasdf fdsa \f\n\r\t\v"); // test default parameters
    BOOST_CHECK_EQUAL(result.size(), 14);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK_EQUAL(result[6], "asdf");
    BOOST_CHECK_EQUAL(result[7], "fdsa");
    BOOST_CHECK(result[3].empty());

    SplitWrapper(result, "", " \n", true);
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK(result[0].empty());

    SplitWrapper(result, "   ", " ", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());

    SplitWrapper(result, "  .", " .", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());

    SplitWrapper(result, "word", " \n", true);
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK_EQUAL(result[0], "word");

    SplitWrapper(result, "simple\ntest", " .\n", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK_EQUAL(result[0], "simple");
    BOOST_CHECK_EQUAL(result[1], "test");

    SplitWrapper(result, "This is a test.", " .", true);
    BOOST_CHECK_EQUAL(result.size(), 5);
    BOOST_CHECK_EQUAL(result[0], "This");
    BOOST_CHECK_EQUAL(result[3], "test");
    BOOST_CHECK(result[4].empty());

    SplitWrapper(result, "This is a test...", " .", true); // the same token should merge
    BOOST_CHECK_EQUAL(result.size(), 5);
    BOOST_CHECK_EQUAL(result[0], "This");
    BOOST_CHECK_EQUAL(result[3], "test");
    BOOST_CHECK(result[4].empty());

    SplitWrapper(result, " \f\n\r\t\vasdf fdsa \f\n\r\t\v", " \f\n\r\t\v", true);
    BOOST_CHECK_EQUAL(result.size(), 4);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK_EQUAL(result[1], "asdf");
    BOOST_CHECK_EQUAL(result[2], "fdsa");
    BOOST_CHECK(result[3].empty());

    // empty separator string should yield the same string again both for compressed and uncompressed version
    SplitWrapper(result, "i lack separators, compressed", "", true);
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK_EQUAL(result[0], "i lack separators, compressed");
    SplitWrapper(result, "i lack separators, uncompressed", "", false);
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK_EQUAL(result[0], "i lack separators, uncompressed");

    // nothing, with compression is 1 empty token
    SplitWrapper(result, "", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK(result[0].empty());
    // nothing, without compression is still 1 empty token
    SplitWrapper(result, "", ",");
    BOOST_CHECK_EQUAL(result.size(), 1);
    BOOST_CHECK(result[0].empty());

    // 2 empty fields, compressed, is 2 empty tokens
    SplitWrapper(result, ",", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    // 2 empty fields, not compressed is also 2 empty tokens
    SplitWrapper(result, ",", ",");
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());

    // 3 empty fields, compressed is 2 empty tokens
    SplitWrapper(result, ",,", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    // 3 empty fields, not compressed is 3 empty tokens
    SplitWrapper(result, ",,", ",");
    BOOST_CHECK_EQUAL(result.size(), 3);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    BOOST_CHECK(result[2].empty());

    // N empty fields, compressed, is always 2 empty tokens
    SplitWrapper(result, ",,,,,", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    // N empty fields, not compressed, is N empty tokens
    SplitWrapper(result, ",,,,,", ",");
    BOOST_CHECK_EQUAL(result.size(), 6);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    BOOST_CHECK(result[2].empty());
    BOOST_CHECK(result[3].empty());
    BOOST_CHECK(result[4].empty());
    BOOST_CHECK(result[5].empty());

    // an odd number of empty fields, plus a non-empty is 2 tokens
    SplitWrapper(result, ",,,hello", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK_EQUAL(result[1], "hello");
    // uncompressed: expect 4 tokens, 3 empty, 1 with "hello"
    SplitWrapper(result, ",,,hello", ",");
    BOOST_CHECK_EQUAL(result.size(), 4);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    BOOST_CHECK(result[2].empty());
    BOOST_CHECK_EQUAL(result[3], "hello");

    // an even number of empty fields plus a non-empty is 2 tokens
    SplitWrapper(result, ",,,,hello", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK_EQUAL(result[1], "hello");
    // uncompressed: expect 5 tokens, 4 empty, 1 with "hello"
    SplitWrapper(result, ",,,,hello", ",");
    BOOST_CHECK_EQUAL(result.size(), 5);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    BOOST_CHECK(result[2].empty());
    BOOST_CHECK(result[3].empty());
    BOOST_CHECK_EQUAL(result[4], "hello");

    // a non-empty, a bunch of empties, and a non-empty is 2 tokens
    SplitWrapper(result, "1,,,,hello", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 2);
    BOOST_CHECK_EQUAL(result[0], "1");
    BOOST_CHECK_EQUAL(result[1], "hello");
    // uncompressed: 5 tokens
    SplitWrapper(result, "1,,,,hello", ",", false);
    BOOST_CHECK_EQUAL(result.size(), 5);
    BOOST_CHECK_EQUAL(result[0], "1");
    BOOST_CHECK(result[1].empty());
    BOOST_CHECK(result[2].empty());
    BOOST_CHECK(result[3].empty());
    BOOST_CHECK_EQUAL(result[4], "hello");

    // compressed: a bunch of empties, a non-empty, a bunch of empties
    SplitWrapper(result, ",,,1,,,,hello", ",", true);
    BOOST_CHECK_EQUAL(result.size(), 3);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK_EQUAL(result[1], "1");
    BOOST_CHECK_EQUAL(result[2], "hello");
    // uncompressed: it's 8 tokens
    SplitWrapper(result, ",,,1,,,,hello", ",", false);
    BOOST_CHECK_EQUAL(result.size(), 8);
    BOOST_CHECK(result[0].empty());
    BOOST_CHECK(result[1].empty());
    BOOST_CHECK(result[2].empty());
    BOOST_CHECK_EQUAL(result[3], "1");
    BOOST_CHECK(result[4].empty());
    BOOST_CHECK(result[5].empty());
    BOOST_CHECK(result[6].empty());
    BOOST_CHECK_EQUAL(result[7], "hello");
}

/// Test string utility functions: replace all
BOOST_AUTO_TEST_CASE(util_ReplaceAll, *boost::unit_test::timeout(5)) {
    auto test_replaceall = [](std::string const &input,
                              std::string const &search,
                              std::string const &format,
                              std::string const &expected){
        std::string input_copy{input};
        ReplaceAll(input_copy, search, format);
        BOOST_CHECK_EQUAL(input_copy, expected);
    };

    // adapted and expanded from boost unit tests for replace_all and erase_all
    test_replaceall("1abc3abc2", "abc", "YYY", "1YYY3YYY2");
    test_replaceall("1abc3abc2", "/", "\\", "1abc3abc2");
    test_replaceall("1abc3abc2", "abc", "Z", "1Z3Z2");
    test_replaceall("1abc3abc2", "abc", "XXXX", "1XXXX3XXXX2");
    test_replaceall("1abc3abc2", "XXXX", "", "1abc3abc2");
    test_replaceall("1abc3abc2", "", "XXXX", "1abc3abc2");
    test_replaceall("1abc3abc2", "", "", "1abc3abc2");
    test_replaceall("1abc3abc2", "abc", "", "132");
    test_replaceall("1abc3abc2", "", "", "1abc3abc2");
    test_replaceall("aaaBBaaaBBaa", "BB", "cBBc", "aaacBBcaaacBBcaa");
    test_replaceall("", "abc", "XXXX", "");
    test_replaceall("", "abc", "", "");
    test_replaceall("", "", "XXXX", "");
    test_replaceall("", "", "", "");
}

/// Test string utility functions: validate
BOOST_AUTO_TEST_CASE(util_ValidAsCString, *boost::unit_test::timeout(5)) {
    using namespace std::string_literals; // since C++14 using std::string literals allows us to embed null characters
    BOOST_CHECK(ValidAsCString("valid"));
    BOOST_CHECK(ValidAsCString(std::string{"valid"}));
    BOOST_CHECK(ValidAsCString(std::string{"valid"s}));
    BOOST_CHECK(ValidAsCString("valid"s));
    BOOST_CHECK(!ValidAsCString("invalid\0"s));
    BOOST_CHECK(!ValidAsCString("\0invalid"s));
    BOOST_CHECK(!ValidAsCString("inv\0alid"s));
    BOOST_CHECK(ValidAsCString(""s));
    BOOST_CHECK(!ValidAsCString("\0"s));
}

BOOST_AUTO_TEST_CASE(util_FormatParseISO8601DateTime) {
    BOOST_CHECK_EQUAL(FormatISO8601DateTime(1317425777),
                      "2011-09-30T23:36:17Z");
    BOOST_CHECK_EQUAL(FormatISO8601DateTime(0), "1970-01-01T00:00:00Z");

    BOOST_CHECK_EQUAL(ParseISO8601DateTime("1970-01-01T00:00:00Z"), 0);
    BOOST_CHECK_EQUAL(ParseISO8601DateTime("1960-01-01T00:00:00Z"), 0);
    BOOST_CHECK_EQUAL(ParseISO8601DateTime("2011-09-30T23:36:17Z"), 1317425777);

    auto time = GetSystemTimeInSeconds();
    BOOST_CHECK_EQUAL(ParseISO8601DateTime(FormatISO8601DateTime(time)), time);
}

BOOST_AUTO_TEST_CASE(util_FormatISO8601Date) {
    BOOST_CHECK_EQUAL(FormatISO8601Date(1317425777), "2011-09-30");
}

struct TestArgsManager : public ArgsManager {
    TestArgsManager() { m_network_only_args.clear(); }
    std::map<std::string, std::vector<std::string>> &GetOverrideArgs() {
        return m_override_args;
    }
    std::map<std::string, std::vector<std::string>> &GetConfigArgs() {
        return m_config_args;
    }
    void ReadConfigString(const std::string str_config) {
        std::istringstream streamConfig(str_config);
        {
            LOCK(cs_args);
            m_config_args.clear();
            m_config_sections.clear();
        }
        std::string error;
        BOOST_REQUIRE(ReadConfigStream(streamConfig, "", error));
    }
    void SetNetworkOnlyArg(const std::string arg) {
        LOCK(cs_args);
        m_network_only_args.insert(arg);
    }
    void
    SetupArgs(const std::vector<std::pair<std::string, unsigned int>> &args) {
        for (const auto &arg : args) {
            AddArg(arg.first, "", arg.second, OptionsCategory::OPTIONS);
        }
    }
    using ArgsManager::cs_args;
    using ArgsManager::m_network;
    using ArgsManager::ReadConfigStream;
};

BOOST_AUTO_TEST_CASE(util_ParseParameters) {
    TestArgsManager testArgs;
    const auto a = std::make_pair("-a", ArgsManager::ALLOW_ANY);
    const auto b = std::make_pair("-b", ArgsManager::ALLOW_ANY);
    const auto ccc = std::make_pair("-ccc", ArgsManager::ALLOW_ANY);
    const auto d = std::make_pair("-d", ArgsManager::ALLOW_ANY);

    const char *argv_test[] = {"-ignored",      "-a", "-b",  "-ccc=argument",
                               "-ccc=multiple", "f",  "-d=e"};

    std::string error;
    testArgs.SetupArgs({a, b, ccc, d});

    BOOST_CHECK(testArgs.ParseParameters(1, (char **)argv_test, error));
    BOOST_CHECK(testArgs.GetOverrideArgs().empty() &&
                testArgs.GetConfigArgs().empty());

    BOOST_CHECK(testArgs.ParseParameters(7, (char **)argv_test, error));
    // expectation: -ignored is ignored (program name argument),
    // -a, -b and -ccc end up in map, -d ignored because it is after
    // a non-option argument (non-GNU option parsing)
    BOOST_CHECK(testArgs.GetOverrideArgs().size() == 3 &&
                testArgs.GetConfigArgs().empty());
    BOOST_CHECK(testArgs.IsArgSet("-a") && testArgs.IsArgSet("-b") &&
                testArgs.IsArgSet("-ccc") && !testArgs.IsArgSet("f") &&
                !testArgs.IsArgSet("-d"));
    BOOST_CHECK(testArgs.GetOverrideArgs().count("-a") &&
                testArgs.GetOverrideArgs().count("-b") &&
                testArgs.GetOverrideArgs().count("-ccc") &&
                !testArgs.GetOverrideArgs().count("f") &&
                !testArgs.GetOverrideArgs().count("-d"));

    BOOST_CHECK(testArgs.GetOverrideArgs()["-a"].size() == 1);
    BOOST_CHECK(testArgs.GetOverrideArgs()["-a"].front() == "");
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].size() == 2);
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].front() == "argument");
    BOOST_CHECK(testArgs.GetOverrideArgs()["-ccc"].back() == "multiple");
    BOOST_CHECK(testArgs.GetArgs("-ccc").size() == 2);
}

BOOST_AUTO_TEST_CASE(util_ParseKeyValue) {
    {
        std::string key = "badarg";
        std::string value;
        BOOST_CHECK(!ParseKeyValue(key, value));
    }
    {
        std::string key = "badarg=v";
        std::string value;
        BOOST_CHECK(!ParseKeyValue(key, value));
    }
    {
        std::string key = "-a";
        std::string value;
        BOOST_CHECK(ParseKeyValue(key, value));
        BOOST_CHECK_EQUAL(key, "-a");
        BOOST_CHECK_EQUAL(value, "");
    }
    {
        std::string key = "-a=1";
        std::string value;
        BOOST_CHECK(ParseKeyValue(key, value));
        BOOST_CHECK_EQUAL(key, "-a");
        BOOST_CHECK_EQUAL(value, "1");
    }
    {
        std::string key = "--b";
        std::string value;
        BOOST_CHECK(ParseKeyValue(key, value));
        BOOST_CHECK_EQUAL(key, "-b");
        BOOST_CHECK_EQUAL(value, "");
    }
    {
        std::string key = "--b=abc";
        std::string value;
        BOOST_CHECK(ParseKeyValue(key, value));
        BOOST_CHECK_EQUAL(key, "-b");
        BOOST_CHECK_EQUAL(value, "abc");
    }
}

BOOST_AUTO_TEST_CASE(util_GetBoolArg) {
    TestArgsManager testArgs;
    const auto a = std::make_pair("-a", ArgsManager::ALLOW_BOOL);
    const auto b = std::make_pair("-b", ArgsManager::ALLOW_BOOL);
    const auto c = std::make_pair("-c", ArgsManager::ALLOW_BOOL);
    const auto d = std::make_pair("-d", ArgsManager::ALLOW_BOOL);
    const auto e = std::make_pair("-e", ArgsManager::ALLOW_BOOL);
    const auto f = std::make_pair("-f", ArgsManager::ALLOW_BOOL);

    const char *argv_test[] = {"ignored", "-a",       "-nob",   "-c=0",
                               "-d=1",    "-e=false", "-f=true"};
    std::string error;
    testArgs.SetupArgs({a, b, c, d, e, f});
    BOOST_CHECK(testArgs.ParseParameters(7, (char **)argv_test, error));

    // Each letter should be set.
    for (const char opt : "abcdef") {
        BOOST_CHECK(testArgs.IsArgSet({'-', opt}) || !opt);
    }

    // Nothing else should be in the map
    BOOST_CHECK(testArgs.GetOverrideArgs().size() == 6 &&
                testArgs.GetConfigArgs().empty());

    // The -no prefix should get stripped on the way in.
    BOOST_CHECK(!testArgs.IsArgSet("-nob"));

    // The -b option is flagged as negated, and nothing else is
    BOOST_CHECK(testArgs.IsArgNegated("-b"));
    BOOST_CHECK(!testArgs.IsArgNegated("-a"));

    // Check expected values.
    BOOST_CHECK(testArgs.GetBoolArg("-a", false) == true);
    BOOST_CHECK(testArgs.GetBoolArg("-b", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-c", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-d", false) == true);
    BOOST_CHECK(testArgs.GetBoolArg("-e", true) == false);
    BOOST_CHECK(testArgs.GetBoolArg("-f", true) == false);
}

BOOST_AUTO_TEST_CASE(util_GetBoolArgEdgeCases) {
    // Test some awful edge cases that hopefully no user will ever exercise.
    TestArgsManager testArgs;

    // Params test
    const auto foo = std::make_pair("-foo", ArgsManager::ALLOW_BOOL);
    const auto bar = std::make_pair("-bar", ArgsManager::ALLOW_BOOL);
    const char *argv_test[] = {"ignored", "-nofoo", "-foo", "-nobar=0"};
    testArgs.SetupArgs({foo, bar});
    std::string error;
    BOOST_CHECK(testArgs.ParseParameters(4, (char **)argv_test, error));

    // This was passed twice, second one overrides the negative setting.
    BOOST_CHECK(!testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "");

    // A double negative is a positive, and not marked as negated.
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "1");

    // Config test
    const char *conf_test = "nofoo=1\nfoo=1\nnobar=0\n";
    BOOST_CHECK(testArgs.ParseParameters(1, (char **)argv_test, error));
    testArgs.ReadConfigString(conf_test);

    // This was passed twice, second one overrides the negative setting,
    // and the value.
    BOOST_CHECK(!testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "1");

    // A double negative is a positive, and does not count as negated.
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "1");

    // Combined test
    const char *combo_test_args[] = {"ignored", "-nofoo", "-bar"};
    const char *combo_test_conf = "foo=1\nnobar=1\n";
    BOOST_CHECK(testArgs.ParseParameters(3, (char **)combo_test_args, error));
    testArgs.ReadConfigString(combo_test_conf);

    // Command line overrides, but doesn't erase old setting
    BOOST_CHECK(testArgs.IsArgNegated("-foo"));
    BOOST_CHECK(testArgs.GetArg("-foo", "xxx") == "0");
    BOOST_CHECK(testArgs.GetArgs("-foo").size() == 0);

    // Command line overrides, but doesn't erase old setting
    BOOST_CHECK(!testArgs.IsArgNegated("-bar"));
    BOOST_CHECK(testArgs.GetArg("-bar", "xxx") == "");
    BOOST_CHECK(testArgs.GetArgs("-bar").size() == 1 &&
                testArgs.GetArgs("-bar").front() == "");
}

BOOST_AUTO_TEST_CASE(util_ReadConfigStream) {
    const char *str_config = "a=\n"
                             "b=1\n"
                             "ccc=argument\n"
                             "ccc=multiple\n"
                             "d=e\n"
                             "nofff=1\n"
                             "noggg=0\n"
                             "h=1\n"
                             "noh=1\n"
                             "noi=1\n"
                             "i=1\n"
                             "sec1.ccc=extend1\n"
                             "\n"
                             "[sec1]\n"
                             "ccc=extend2\n"
                             "d=eee\n"
                             "h=1\n"
                             "[sec2]\n"
                             "ccc=extend3\n"
                             "iii=2\n";

    TestArgsManager test_args;
    const auto a = std::make_pair("-a", ArgsManager::ALLOW_BOOL);
    const auto b = std::make_pair("-b", ArgsManager::ALLOW_BOOL);
    const auto ccc = std::make_pair("-ccc", ArgsManager::ALLOW_STRING);
    const auto d = std::make_pair("-d", ArgsManager::ALLOW_STRING);
    const auto e = std::make_pair("-e", ArgsManager::ALLOW_ANY);
    const auto fff = std::make_pair("-fff", ArgsManager::ALLOW_BOOL);
    const auto ggg = std::make_pair("-ggg", ArgsManager::ALLOW_BOOL);
    const auto h = std::make_pair("-h", ArgsManager::ALLOW_BOOL);
    const auto i = std::make_pair("-i", ArgsManager::ALLOW_BOOL);
    const auto iii = std::make_pair("-iii", ArgsManager::ALLOW_INT);
    test_args.SetupArgs({a, b, ccc, d, e, fff, ggg, h, i, iii});

    test_args.ReadConfigString(str_config);
    // expectation: a, b, ccc, d, fff, ggg, h, i end up in map
    // so do sec1.ccc, sec1.d, sec1.h, sec2.ccc, sec2.iii

    BOOST_CHECK(test_args.GetOverrideArgs().empty());
    BOOST_CHECK(test_args.GetConfigArgs().size() == 13);

    BOOST_CHECK(test_args.GetConfigArgs().count("-a") &&
                test_args.GetConfigArgs().count("-b") &&
                test_args.GetConfigArgs().count("-ccc") &&
                test_args.GetConfigArgs().count("-d") &&
                test_args.GetConfigArgs().count("-fff") &&
                test_args.GetConfigArgs().count("-ggg") &&
                test_args.GetConfigArgs().count("-h") &&
                test_args.GetConfigArgs().count("-i"));
    BOOST_CHECK(test_args.GetConfigArgs().count("-sec1.ccc") &&
                test_args.GetConfigArgs().count("-sec1.h") &&
                test_args.GetConfigArgs().count("-sec2.ccc") &&
                test_args.GetConfigArgs().count("-sec2.iii"));

    BOOST_CHECK(test_args.IsArgSet("-a") && test_args.IsArgSet("-b") &&
                test_args.IsArgSet("-ccc") && test_args.IsArgSet("-d") &&
                test_args.IsArgSet("-fff") && test_args.IsArgSet("-ggg") &&
                test_args.IsArgSet("-h") && test_args.IsArgSet("-i") &&
                !test_args.IsArgSet("-zzz") && !test_args.IsArgSet("-iii"));

    BOOST_CHECK(test_args.GetArg("-a", "xxx") == "" &&
                test_args.GetArg("-b", "xxx") == "1" &&
                test_args.GetArg("-ccc", "xxx") == "argument" &&
                test_args.GetArg("-d", "xxx") == "e" &&
                test_args.GetArg("-fff", "xxx") == "0" &&
                test_args.GetArg("-ggg", "xxx") == "1" &&
                test_args.GetArg("-h", "xxx") == "0" &&
                test_args.GetArg("-i", "xxx") == "1" &&
                test_args.GetArg("-zzz", "xxx") == "xxx" &&
                test_args.GetArg("-iii", "xxx") == "xxx");

    for (const bool def : {false, true}) {
        BOOST_CHECK(test_args.GetBoolArg("-a", def) &&
                    test_args.GetBoolArg("-b", def) &&
                    !test_args.GetBoolArg("-ccc", def) &&
                    !test_args.GetBoolArg("-d", def) &&
                    !test_args.GetBoolArg("-fff", def) &&
                    test_args.GetBoolArg("-ggg", def) &&
                    !test_args.GetBoolArg("-h", def) &&
                    test_args.GetBoolArg("-i", def) &&
                    test_args.GetBoolArg("-zzz", def) == def &&
                    test_args.GetBoolArg("-iii", def) == def);
    }

    BOOST_CHECK(test_args.GetArgs("-a").size() == 1 &&
                test_args.GetArgs("-a").front() == "");
    BOOST_CHECK(test_args.GetArgs("-b").size() == 1 &&
                test_args.GetArgs("-b").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2 &&
                test_args.GetArgs("-ccc").front() == "argument" &&
                test_args.GetArgs("-ccc").back() == "multiple");
    BOOST_CHECK(test_args.GetArgs("-fff").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-nofff").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-ggg").size() == 1 &&
                test_args.GetArgs("-ggg").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-noggg").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-h").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-noh").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-i").size() == 1 &&
                test_args.GetArgs("-i").front() == "1");
    BOOST_CHECK(test_args.GetArgs("-noi").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-zzz").size() == 0);

    BOOST_CHECK(!test_args.IsArgNegated("-a"));
    BOOST_CHECK(!test_args.IsArgNegated("-b"));
    BOOST_CHECK(!test_args.IsArgNegated("-ccc"));
    BOOST_CHECK(!test_args.IsArgNegated("-d"));
    BOOST_CHECK(test_args.IsArgNegated("-fff"));
    BOOST_CHECK(!test_args.IsArgNegated("-ggg"));
    // last setting takes precedence
    BOOST_CHECK(test_args.IsArgNegated("-h"));
    // last setting takes precedence
    BOOST_CHECK(!test_args.IsArgNegated("-i"));
    BOOST_CHECK(!test_args.IsArgNegated("-zzz"));

    // Test sections work
    test_args.SelectConfigNetwork("sec1");

    // same as original
    BOOST_CHECK(test_args.GetArg("-a", "xxx") == "" &&
                test_args.GetArg("-b", "xxx") == "1" &&
                test_args.GetArg("-fff", "xxx") == "0" &&
                test_args.GetArg("-ggg", "xxx") == "1" &&
                test_args.GetArg("-zzz", "xxx") == "xxx" &&
                test_args.GetArg("-iii", "xxx") == "xxx");
    // d is overridden
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "eee");
    // section-specific setting
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "1");
    // section takes priority for multiple values
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "extend1");
    // check multiple values works
    const std::vector<std::string> sec1_ccc_expected = {"extend1", "extend2",
                                                        "argument", "multiple"};
    const auto &sec1_ccc_res = test_args.GetArgs("-ccc");
    BOOST_CHECK_EQUAL_COLLECTIONS(sec1_ccc_res.begin(), sec1_ccc_res.end(),
                                  sec1_ccc_expected.begin(),
                                  sec1_ccc_expected.end());

    test_args.SelectConfigNetwork("sec2");

    // same as original
    BOOST_CHECK(test_args.GetArg("-a", "xxx") == "" &&
                test_args.GetArg("-b", "xxx") == "1" &&
                test_args.GetArg("-d", "xxx") == "e" &&
                test_args.GetArg("-fff", "xxx") == "0" &&
                test_args.GetArg("-ggg", "xxx") == "1" &&
                test_args.GetArg("-zzz", "xxx") == "xxx" &&
                test_args.GetArg("-h", "xxx") == "0");
    // section-specific setting
    BOOST_CHECK(test_args.GetArg("-iii", "xxx") == "2");
    // section takes priority for multiple values
    BOOST_CHECK(test_args.GetArg("-ccc", "xxx") == "extend3");
    // check multiple values works
    const std::vector<std::string> sec2_ccc_expected = {"extend3", "argument",
                                                        "multiple"};
    const auto &sec2_ccc_res = test_args.GetArgs("-ccc");
    BOOST_CHECK_EQUAL_COLLECTIONS(sec2_ccc_res.begin(), sec2_ccc_res.end(),
                                  sec2_ccc_expected.begin(),
                                  sec2_ccc_expected.end());

    // Test section only options

    test_args.SetNetworkOnlyArg("-d");
    test_args.SetNetworkOnlyArg("-ccc");
    test_args.SetNetworkOnlyArg("-h");

    test_args.SelectConfigNetwork(CBaseChainParams::MAIN);
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "e");
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");

    test_args.SelectConfigNetwork("sec1");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "eee");
    BOOST_CHECK(test_args.GetArgs("-d").size() == 1);
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 2);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "1");

    test_args.SelectConfigNetwork("sec2");
    BOOST_CHECK(test_args.GetArg("-d", "xxx") == "xxx");
    BOOST_CHECK(test_args.GetArgs("-d").size() == 0);
    BOOST_CHECK(test_args.GetArgs("-ccc").size() == 1);
    BOOST_CHECK(test_args.GetArg("-h", "xxx") == "0");
}

BOOST_AUTO_TEST_CASE(util_GetArg) {
    TestArgsManager testArgs;
    testArgs.GetOverrideArgs().clear();
    testArgs.GetOverrideArgs()["strtest1"] = {"string..."};
    // strtest2 undefined on purpose
    testArgs.GetOverrideArgs()["inttest1"] = {"12345"};
    testArgs.GetOverrideArgs()["inttest2"] = {"81985529216486895"};
    // inttest3 undefined on purpose
    testArgs.GetOverrideArgs()["booltest1"] = {""};
    // booltest2 undefined on purpose
    testArgs.GetOverrideArgs()["booltest3"] = {"0"};
    testArgs.GetOverrideArgs()["booltest4"] = {"1"};

    // priorities
    testArgs.GetOverrideArgs()["pritest1"] = {"a", "b"};
    testArgs.GetConfigArgs()["pritest2"] = {"a", "b"};
    testArgs.GetOverrideArgs()["pritest3"] = {"a"};
    testArgs.GetConfigArgs()["pritest3"] = {"b"};
    testArgs.GetOverrideArgs()["pritest4"] = {"a", "b"};
    testArgs.GetConfigArgs()["pritest4"] = {"c", "d"};

    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest1", -1), 12345);
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest2", -1), 81985529216486895LL);
    BOOST_CHECK_EQUAL(testArgs.GetArg("inttest3", -1), -1);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", false), true);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest2", false), false);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest3", false), false);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest4", false), true);

    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest1", "default"), "b");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest2", "default"), "a");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest3", "default"), "a");
    BOOST_CHECK_EQUAL(testArgs.GetArg("pritest4", "default"), "b");
}

BOOST_AUTO_TEST_CASE(util_ClearArg) {
    TestArgsManager testArgs;

    // Clear single string arg
    testArgs.GetOverrideArgs()["strtest1"] = {"string..."};
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "string...");
    testArgs.ClearArg("strtest1");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "default");

    // Clear boolean arg
    testArgs.GetOverrideArgs()["booltest1"] = {"1"};
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", false), true);
    testArgs.ClearArg("booltest1");
    BOOST_CHECK_EQUAL(testArgs.GetArg("booltest1", false), false);

    // Clear config args only
    testArgs.GetConfigArgs()["strtest2"].push_back("string...");
    testArgs.GetConfigArgs()["strtest2"].push_back("...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 2);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").back(), "...gnirts");
    testArgs.ClearArg("strtest2");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 0);

    // Clear both cli args and config args
    testArgs.GetOverrideArgs()["strtest3"].push_back("cli string...");
    testArgs.GetOverrideArgs()["strtest3"].push_back("...gnirts ilc");
    testArgs.GetConfigArgs()["strtest3"].push_back("string...");
    testArgs.GetConfigArgs()["strtest3"].push_back("...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest3", "default"), "...gnirts ilc");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest3").size(), 4);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest3").front(), "cli string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest3").back(), "...gnirts");
    testArgs.ClearArg("strtest3");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest3", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest3").size(), 0);
}

BOOST_AUTO_TEST_CASE(util_SetArg) {
    TestArgsManager testArgs;

    // SoftSetArg
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.SoftSetArg("strtest1", "string..."), true);
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest1").size(), 1);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest1").front(), "string...");
    BOOST_CHECK_EQUAL(testArgs.SoftSetArg("strtest1", "...gnirts"), false);
    testArgs.ClearArg("strtest1");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.SoftSetArg("strtest1", "...gnirts"), true);
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest1", "default"), "...gnirts");

    // SoftSetBoolArg
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", false), false);
    BOOST_CHECK_EQUAL(testArgs.SoftSetBoolArg("booltest1", true), true);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", false), true);
    BOOST_CHECK_EQUAL(testArgs.SoftSetBoolArg("booltest1", false), false);
    testArgs.ClearArg("booltest1");
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", true), true);
    BOOST_CHECK_EQUAL(testArgs.SoftSetBoolArg("booltest1", false), true);
    BOOST_CHECK_EQUAL(testArgs.GetBoolArg("booltest1", true), false);

    // ForceSetArg
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "default");
    testArgs.ForceSetArg("strtest2", "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 1);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "string...");
    testArgs.ForceSetArg("strtest2", "...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 1);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "...gnirts");

    // ForceSetMultiArg
    testArgs.ForceSetMultiArg("strtest2", "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 2);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").back(), "string...");
    testArgs.ClearArg("strtest2");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "default");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 0);
    testArgs.ForceSetMultiArg("strtest2", "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 1);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "string...");
    testArgs.ForceSetMultiArg("strtest2", "one more thing...");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"),
                      "one more thing...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 2);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "string...");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").back(), "one more thing...");
    // If there are multi args, ForceSetArg should erase them
    testArgs.ForceSetArg("strtest2", "...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArg("strtest2", "default"), "...gnirts");
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").size(), 1);
    BOOST_CHECK_EQUAL(testArgs.GetArgs("strtest2").front(), "...gnirts");
}

BOOST_AUTO_TEST_SUITE_END()
