#!/usr/bin/env bash
#
# Copyright (c) 2018-2020 The Bitcoin Core developers
# Copyright (c) 2021 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Check include guards.

export LC_ALL=C
HEADER_ID_PREFIX="BITCOIN_"
HEADER_ID_SUFFIX="_H"

EXIT_CODE=0

mapfile -d '' file_list < <(git ls-files -z -- '*.h' ':!:src/crypto/ctaes' ':!:src/leveldb' ':!:src/secp256k1' ':!:src/tinyformat.h' ':!:src/univalue')
for HEADER_FILE in "${file_list[@]}"; do
    # check for "#pragma once" - if present, don't verify include guard format - but only if preceded by whitespace, i.e. commented pragmas fail the test
    if grep -q '^[[:space:]]*#pragma once' "${HEADER_FILE}"; then
        continue
    fi

    HEADER_ID_BASE=$(cut -f2- -d/ <<< "${HEADER_FILE}" | sed "s/\.h$//g" | tr / _ | tr - _ | tr "[:lower:]" "[:upper:]")
    HEADER_ID="${HEADER_ID_PREFIX}${HEADER_ID_BASE}${HEADER_ID_SUFFIX}"
    if [[ $(grep --count --extended-regexp "^#(ifndef|define|endif //) ${HEADER_ID}" "${HEADER_FILE}") != 3 ]]; then
        echo "${HEADER_FILE} seems to be missing the expected include guard:"
        echo "  #ifndef ${HEADER_ID}"
        echo "  #define ${HEADER_ID}"
        echo "  ..."
        echo "  #endif // ${HEADER_ID}"
        echo
        EXIT_CODE=1
    fi
done
exit ${EXIT_CODE}
