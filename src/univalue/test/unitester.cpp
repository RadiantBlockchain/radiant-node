// Copyright 2014 BitPay Inc.
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <cassert>
#include <string>
#include "univalue.h"

#ifndef JSON_TEST_SRC
#error JSON_TEST_SRC must point to test source directory
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

std::string srcdir(JSON_TEST_SRC);
static bool test_failed = false;

#define r_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s read failed\n", filename.c_str()); } }
#define w_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s write failed\n", filename.c_str()); } }
#define f_assert(expr) { if (!(expr)) { test_failed = true; fprintf(stderr, "%s failed\n", __func__); } }

static std::string rtrim(std::string s)
{
    s.erase(s.find_last_not_of(" \n\r\t")+1);
    return s;
}

static void runtest(const std::string& filename, const std::string& jdata)
{
        std::string prefix = filename.substr(0, 4);

        bool wantPrettyRoundTrip = prefix == "pret";
        bool wantRoundTrip = wantPrettyRoundTrip || prefix == "roun";
        bool wantPass = wantRoundTrip || prefix == "pass";
        bool wantFail = prefix == "fail";

        assert(wantPass || wantFail);

        UniValue val;
        bool testResult = val.read(jdata);

        r_assert(testResult == wantPass);
        if (wantRoundTrip) {
            std::string odata = UniValue::stringify(val, wantPrettyRoundTrip ? 4 : 0);
            w_assert(odata == rtrim(jdata));
        }
}

static void runtest_file(const char *filename_)
{
        std::string basename(filename_);
        std::string filename = srcdir + "/" + basename;
        FILE *f = fopen(filename.c_str(), "r");
        assert(f != NULL);

        std::string jdata;

        char buf[4096];
        while (!feof(f)) {
                int bread = fread(buf, 1, sizeof(buf), f);
                assert(!ferror(f));

                std::string s(buf, bread);
                jdata += s;
        }

        assert(!ferror(f));
        fclose(f);

        runtest(basename, jdata);
}

static const char *filenames[] = {
        "fail1.json",
        "fail2.json",
        "fail3.json",
        "fail4.json",   // extra comma
        "fail5.json",
        "fail6.json",
        "fail7.json",
        "fail8.json",
        "fail9.json",   // extra comma
        "fail10.json",
        "fail11.json",
        "fail12.json",
        "fail13.json",
        "fail14.json",
        "fail15.json",
        "fail16.json",
        "fail17.json",
        "fail19.json",
        "fail20.json",
        "fail21.json",
        "fail22.json",
        "fail23.json",
        "fail24.json",
        "fail25.json",
        "fail26.json",
        "fail27.json",
        "fail28.json",
        "fail29.json",
        "fail30.json",
        "fail31.json",
        "fail32.json",
        "fail33.json",
        "fail34.json",
        "fail35.json",
        "fail36.json",
        "fail37.json",
        "fail38.json",  // invalid unicode: only first half of surrogate pair
        "fail39.json",  // invalid unicode: only second half of surrogate pair
        "fail40.json",  // invalid unicode: broken UTF-8
        "fail41.json",  // invalid unicode: unfinished UTF-8
        "fail42.json",  // valid json with garbage following a nul byte
        "fail44.json",  // unterminated string
        "fail45.json",  // nested beyond max depth
        "fail46.json",  // nested beyond max depth, with whitespace
        "fail47.json",  // buffer consisting of only a hyphen (chars: {'-', '\0'})
        "fail48.json",  // -00 is not a valid JSON number
        "fail49.json",  // 0123 is not a valid JSON number
        "fail50.json",  // -1. is not a valid JSON number (no ending in decimals)
        "fail51.json",  // 1.3e+ is not valid (must have a number after the "e+" part)
        "fail52.json",  // reject -[non-digit]
        "pass1.json",
        "round1.json",  // round-trip test
        "round2.json",  // unicode
        "round3.json",  // bare string
        "round4.json",  // bare number
        "round5.json",  // bare true
        "round6.json",  // bare false
        "round7.json",  // bare null
        "round8.json",  // nested at max depth
        "round9.json",  // accept bare buffer containing only "-0"
        "pretty1.json",
        "pretty2.json",
};

// Test \u handling
void unescape_unicode_test()
{
    UniValue val;
    bool testResult;
    // Escaped ASCII (quote)
    testResult = val.read("[\"\\u0022\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\"");
    // Escaped Basic Plane character, two-byte UTF-8
    testResult = val.read("[\"\\u0191\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xc6\x91");
    // Escaped Basic Plane character, three-byte UTF-8
    testResult = val.read("[\"\\u2191\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xe2\x86\x91");
    // Escaped Supplementary Plane character U+1d161
    testResult = val.read("[\"\\ud834\\udd61\"]");
    f_assert(testResult);
    f_assert(val[0].get_str() == "\xf0\x9d\x85\xa1");
}

int main (int argc, char *argv[])
{
    for (unsigned int fidx = 0; fidx < ARRAY_SIZE(filenames); fidx++) {
        runtest_file(filenames[fidx]);
    }

    unescape_unicode_test();

    return test_failed ? 1 : 0;
}

