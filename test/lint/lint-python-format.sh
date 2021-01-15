#!/usr/bin/env bash
#
# Copyright (c) 2021 The Bitcoin developers.
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C

EXIT_CODE=0
for f in $(git ls-files "*py" | grep -v "lint-python-format.py"); do
    if ! test/lint/lint-python-format.py ${f}; then
        echo "^^^ in file ${f}"
        EXIT_CODE=1
    fi
done

exit ${EXIT_CODE}
