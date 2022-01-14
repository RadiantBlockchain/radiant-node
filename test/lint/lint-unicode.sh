#!/usr/bin/env bash
#
# Copyright (c) 2021 The Bitcoin developers.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

EXIT_CODE=0
mapfile -d '' file_list < <(git ls-files -z -- '*.py' '*.cpp' '*.h' '*.c' '*.json' 'CMakeLists.txt' '*.sh' '*.am')
for f in "${file_list[@]}"; do
    if ! ALLOW="440,430" DECODE_ERRORS=0 "${LINT_BINDIR}/native-unicode_src_linter" "${f}"; then
        EXIT_CODE=1
    fi
done

exit "${EXIT_CODE}"
