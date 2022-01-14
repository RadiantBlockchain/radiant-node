#!/usr/bin/env bash
#
# Copyright (c) 2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
#
# Lint YAML files.

export LC_ALL=C

SCRIPT_DIR="$(dirname "$0")"
LINT_CONFIG="$SCRIPT_DIR/lint-yaml-config.yml"

if ! command -v yamllint &>/dev/null; then
  echo "WARNING: yamllint not found. Skipping lint-yaml."
  exit 0
fi

EXIT_CODE=0

# Filter out yaml files belonging to upstream projects.
mapfile -d '' FILES < <(git ls-files -z -- '*.yml' '*.yaml' '*.yml.in' '*.yaml.in' ':!:contrib' ':!:src/secp256k1' ':!:src/leveldb')
for filename in "${FILES[@]}"; do
  echo "Linting $filename"
  if ! yamllint -c "$LINT_CONFIG" -- "$filename"; then
    EXIT_CODE=1
  fi
done

exit $EXIT_CODE
