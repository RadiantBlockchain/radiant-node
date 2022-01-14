#!/usr/bin/env bash
#
# Copyright (c) 2022 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Make sure all shell scripts begin with "#!/usr/bin/env bash"

export LC_ALL=C

EXPECTED_WRONG_SHEBANG_SCRIPTS=(
    cmake/utils/test_wrapper.sh
    contrib/macdeploy/detached-sig-apply.sh
    contrib/macdeploy/detached-sig-create.sh
    share/genbuild.sh
    src/secp256k1/autogen.sh
    src/univalue/autogen.sh
    test/lint/git-subtree-check.sh
)

EXIT_CODE=0
mapfile -d '' file_list < <(git grep -z -L '#!/usr/bin/env bash' -- '*.sh' '*.in.sh')
for SHELL_SCRIPT in "${file_list[@]}"; do
    IS_EXPECTED_SCRIPT=0
    for EXPECTED_WRONG_SHEBANG_SCRIPT in "${EXPECTED_WRONG_SHEBANG_SCRIPTS[@]}"; do
        if [[ "${SHELL_SCRIPT}" == "${EXPECTED_WRONG_SHEBANG_SCRIPT}" ]]; then
            IS_EXPECTED_SCRIPT=1
            break
        fi
    done
    if [ $IS_EXPECTED_SCRIPT -eq 0 ]; then
        if [ "$EXIT_CODE" -eq 0 ] ; then
            echo "New script(s) with the wrong shebang found:"
        fi
        EXIT_CODE=1
        printf '%q:' "$SHELL_SCRIPT"
        head -n1 "$SHELL_SCRIPT"
        echo
    fi
done
exit ${EXIT_CODE}
