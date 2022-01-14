#!/usr/bin/env bash
#
# Copyright (c) 2021 The Bitcoin developers.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

EXIT_CODE=0

mapfile -d '' file_list < <(git ls-files -z -- '*.cpp' '*.h' ':!:src/univalue' ':!:src/leveldb')
for f in "${file_list[@]}"; do
    if ! test/lint/lint-format-strings.py "${f}"; then
        echo "^^^ in file ${f}"
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
