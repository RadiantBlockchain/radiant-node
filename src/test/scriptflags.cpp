// Copyright (c) 2017-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/interpreter.h>

#include <test/scriptflags.h>
#include <util/string.h>

#include <boost/test/unit_test.hpp>

#include <map>
#include <vector>

static std::map<std::string, uint32_t> mapFlagNames = {
    {"NONE", SCRIPT_VERIFY_NONE},
    {"STRICTENC", SCRIPT_VERIFY_STRICTENC},
    {"DERSIG", SCRIPT_VERIFY_DERSIG},
    {"LOW_S", SCRIPT_VERIFY_LOW_S},
    {"SIGPUSHONLY", SCRIPT_VERIFY_SIGPUSHONLY},
    {"MINIMALDATA", SCRIPT_VERIFY_MINIMALDATA},
    {"DISCOURAGE_UPGRADABLE_NOPS", SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS},
    {"CLEANSTACK", SCRIPT_VERIFY_CLEANSTACK},
    {"MINIMALIF", SCRIPT_VERIFY_MINIMALIF},
    {"NULLFAIL", SCRIPT_VERIFY_NULLFAIL},
    {"CHECKLOCKTIMEVERIFY", SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY},
    {"CHECKSEQUENCEVERIFY", SCRIPT_VERIFY_CHECKSEQUENCEVERIFY},
    {"SIGHASH_FORKID", SCRIPT_ENABLE_SIGHASH_FORKID},
    {"DISALLOW_SEGWIT_RECOVERY", SCRIPT_DISALLOW_SEGWIT_RECOVERY},
    {"SCHNORR_MULTISIG", SCRIPT_ENABLE_SCHNORR_MULTISIG},
    {"INPUT_SIGCHECKS", SCRIPT_VERIFY_INPUT_SIGCHECKS},
    {"64_BIT_INTEGERS", SCRIPT_64_BIT_INTEGERS},
    {"NATIVE_INTROSPECTION", SCRIPT_NATIVE_INTROSPECTION},
};

uint32_t ParseScriptFlags(const std::string &strFlags) {
    if (strFlags.empty()) {
        return 0;
    }

    uint32_t flags = 0;
    std::vector<std::string> words;
    Split(words, strFlags, ",");

    for (const std::string &word : words) {
        if (!mapFlagNames.count(word)) {
            BOOST_ERROR("Bad test: unknown verification flag '" << word << "'");
        }
        flags |= mapFlagNames[word];
    }

    return flags;
}

std::string FormatScriptFlags(uint32_t flags) {
    if (flags == 0) {
        return "";
    }

    std::string ret;
    std::map<std::string, uint32_t>::const_iterator it = mapFlagNames.begin();
    while (it != mapFlagNames.end()) {
        if (flags & it->second) {
            ret += it->first + ",";
        }
        it++;
    }

    return ret.substr(0, ret.size() - 1);
}
