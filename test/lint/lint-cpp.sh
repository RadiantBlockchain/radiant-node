#!/usr/bin/env bash
#
# Copyright (c) 2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check for various C++ code patterns we want to avoid.

export LC_ALL=C

EXIT_CODE=0

# Search for PATTERN ($1) and if found, outputs PATTERN_FOUND_MESSAGE ($2)
# and the offending matches, and sets global EXIT_CODE to 1.
# PATTERN_FOUND_MESSAGE should include some helpful hints on how to fix the
# problem, if possible.
# Variable number of arguments from $3 onwards are taken as file globs
# which are passed to the 'git grep' search command.
search_and_report_if_found() {
    PATTERN="$1"
    PATTERN_FOUND_MESSAGE="$2"
    shift 2
    OUTPUT=$(git grep -E "$PATTERN" -- $@)
    if [[ ${OUTPUT} != "" ]]; then
        echo "$PATTERN_FOUND_MESSAGE"
        echo
        echo "${OUTPUT}"
        echo
        EXIT_CODE=1
    fi
}

search_and_report_if_found "boost::bind" \
                           "Use of boost::bind detected. Use std::bind instead." \
                           "*.cpp" "*.h"

# C++17 language features we do not want to be used right now since they are
# not supported on all platforms yet:
search_and_report_if_found "std::filesystem" \
   "Use of std::filesystem detected. This is a problem on OSX 10.14 (gitian build). Use boost::filesystem instead." \
   "*.cpp" "*.h"

search_and_report_if_found "std::variant" \
   "Use of std::variant detected. This is a problem on OSX 10.14 (gitian build). Use boost::variant instead." \
   "*.cpp" "*.h"
exit ${EXIT_CODE}
