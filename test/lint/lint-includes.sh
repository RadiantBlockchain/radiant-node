#!/usr/bin/env bash
#
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check for duplicate includes.
# Guard against accidental introduction of new Boost dependencies.
# Check includes: Check for duplicate includes. Enforce bracket syntax includes.

export LC_ALL=C
IGNORES=(':!:src/leveldb' ':!:src/secp256k1' ':!:src/univalue' ':!:src/crc32c')

# cd to root folder of git repo for git ls-files to work properly
cd "$(dirname "$0")/../.." || exit 1

filter_suffix() {
    git ls-files -z -- "src/*.${1}" "${IGNORES[@]}"
}

EXIT_CODE=0

mapfile -d '' file_list < <(filter_suffix h)
for HEADER_FILE in "${file_list[@]}"; do
    DUPLICATE_INCLUDES_IN_HEADER_FILE=$(grep -E "^#include " < "${HEADER_FILE}" | sort | uniq -d)
    if [[ ${DUPLICATE_INCLUDES_IN_HEADER_FILE} != "" ]]; then
        echo "Duplicate include(s) in $(printf '%q\n' "${HEADER_FILE}"):"
        echo "${DUPLICATE_INCLUDES_IN_HEADER_FILE}"
        echo
        EXIT_CODE=1
    fi
done

mapfile -d '' file_list < <(filter_suffix cpp)
for CPP_FILE in "${file_list[@]}"; do
    DUPLICATE_INCLUDES_IN_CPP_FILE=$(grep -E "^#include " < "${CPP_FILE}" | sort | uniq -d)
    if [[ ${DUPLICATE_INCLUDES_IN_CPP_FILE} != "" ]]; then
        echo "Duplicate include(s) in $(printf '%q\n' "${CPP_FILE}"):"
        echo "${DUPLICATE_INCLUDES_IN_CPP_FILE}"
        echo
        EXIT_CODE=1
    fi
done

mapfile -d '' INCLUDED_CPP_FILES < <(git grep -l -z -E "^#include [<\"][^>\"]+\.cpp[>\"]" -- "*.cpp" "*.h")
if [[ "${#INCLUDED_CPP_FILES[@]}" -ne 0 ]]; then
    echo "The following files #include .cpp files:"
    grep -EHn "^#include [<\"][^>\"]+\.cpp[>\"]" -- "${INCLUDED_CPP_FILES[@]}"
    echo
    EXIT_CODE=1
fi

EXPECTED_BOOST_INCLUDES=(
    boost/chrono/chrono.hpp
    boost/date_time/posix_time/posix_time.hpp
    boost/filesystem.hpp
    boost/filesystem/fstream.hpp
    boost/multi_index/hashed_index.hpp
    boost/multi_index/member.hpp
    boost/multi_index/ordered_index.hpp
    boost/multi_index/sequenced_index.hpp
    boost/multi_index_container.hpp
    boost/noncopyable.hpp
    boost/preprocessor/cat.hpp
    boost/preprocessor/stringize.hpp
    boost/range/adaptor/sliced.hpp
    boost/range/iterator.hpp
    boost/signals2/connection.hpp
    boost/signals2/last_value.hpp
    boost/signals2/signal.hpp
    boost/test/unit_test.hpp
    boost/thread.hpp
    boost/thread/condition_variable.hpp
    boost/thread/locks.hpp
    boost/thread/shared_mutex.hpp
    boost/thread/thread.hpp
    boost/variant.hpp
    boost/variant/apply_visitor.hpp
    boost/variant/static_visitor.hpp
)

file_list=($(git grep '^#include <boost/' -- "*.cpp" "*.h" | cut -f2 -d: | cut -f2 -d'<' | cut -f1 -d'>' | sort -u))
for BOOST_INCLUDE in "${file_list[@]}"; do
    IS_EXPECTED_INCLUDE=0
    for EXPECTED_BOOST_INCLUDE in "${EXPECTED_BOOST_INCLUDES[@]}"; do
        if [[ "${BOOST_INCLUDE}" == "${EXPECTED_BOOST_INCLUDE}" ]]; then
            IS_EXPECTED_INCLUDE=1
            break
        fi
    done
    if [[ ${IS_EXPECTED_INCLUDE} == 0 ]]; then
        EXIT_CODE=1
        echo "A new Boost dependency in the form of \"${BOOST_INCLUDE}\" appears to have been introduced:"
        git grep "${BOOST_INCLUDE}" -- "*.cpp" "*.h"
        echo
    fi
done

for EXPECTED_BOOST_INCLUDE in "${EXPECTED_BOOST_INCLUDES[@]}"; do
    if ! git grep -q "^#include <${EXPECTED_BOOST_INCLUDE}>" -- "*.cpp" "*.h"; then
        echo "Good job! The Boost dependency \"${EXPECTED_BOOST_INCLUDE}\" is no longer used."
        echo "Please remove it from EXPECTED_BOOST_INCLUDES in $0"
        echo "to make sure this dependency is not accidentally reintroduced."
        echo
        EXIT_CODE=1
    fi
done

mapfile -d '' QUOTE_SYNTAX_INCLUDES < <(git grep -l -z '^#include "' -- "*.cpp" "*.h" "${IGNORES[@]}")
if [[ "${#QUOTE_SYNTAX_INCLUDES[@]}" -ne 0 ]]; then
    echo "Please use bracket syntax includes (\"#include <foo.h>\") instead of quote syntax includes:"
    grep -Hn '^#include "' -- "${QUOTE_SYNTAX_INCLUDES[@]}"
    echo
    EXIT_CODE=1
fi

exit ${EXIT_CODE}
