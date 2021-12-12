#!/usr/bin/env bash
#
# Copyright (c) 2021 The Bitcoin developers.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

EXIT_CODE=0
OLDIFS=$IFS
IFS=$'\n'
filelist=($(git ls-files "*py" | grep -Fv "lint-python-format.py"))
IFS=$OLDIFS
for f in "${filelist[@]}"; do
    if ! test/lint/lint-python-format.py "${f}"; then
        echo "^^^ in file ${f}"
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
