#!/usr/bin/env bash
# Copyright (c) 2016-2019 The Bitcoin Core developers
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BITCOIND="$BUILDDIR/src/bitcoind"
BITCOINQT="$BUILDDIR/src/qt/bitcoin-qt"
BITCOINCLI="$BUILDDIR/src/bitcoin-cli"
BITCOINTX="$BUILDDIR/src/bitcoin-tx"
BITCOINSEEDER="$BUILDDIR/src/seeder/bitcoin-seeder"

for cmd in "$BITCOIND" "$BITCOINQT" "$BITCOINCLI" "$BITCOINTX" "$BITCOINSEEDER"; do
  [ ! -x "$cmd" ] && echo "$cmd not found or not executable." && exit 1
done

# The autodetected version git tag can screw up manpage output a little bit
read -r -a BTCVER <<< "$($BITCOINCLI --version | head -n1 | awk -F'[ -]' '{ print $7, $8 }')"

# Create a footer file with copyright content.
# This gets autodetected fine for bitcoind if --version-string is not set,
# but has different outcomes for bitcoin-qt and bitcoin-cli.
echo "[COPYRIGHT]" > footer.h2m
"$BITCOIND" --version | sed -n '1!p' >> footer.h2m

for cmd in "$BITCOIND" "$BITCOINQT"; do
  cmdname="${cmd##*/}"
  "$TOPDIR/contrib/devtools/cli-help-to-markdown.py" "$($cmd -?? -lang=en_US)" > "$TOPDIR/doc/cli/$cmdname.md"
  sed -i "s/\-${BTCVER[1]}\(\-dirty\)\?//g" "$TOPDIR/doc/cli/$cmdname.md"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o "$TOPDIR/doc/man/$cmdname.1" --help-option="-? -lang=en_US" --version-option="-version -lang=en_US" "$cmd"
  sed -i "s/\\\-${BTCVER[1]}\(\\\-dirty\)\?//g" "$TOPDIR/doc/man/$cmdname.1"
done

for cmd in "$BITCOINCLI" "$BITCOINTX" "$BITCOINSEEDER"; do
  cmdname="${cmd##*/}"
  "$TOPDIR/contrib/devtools/cli-help-to-markdown.py" "$($cmd -?)" > "$TOPDIR/doc/cli/$cmdname.md"
  sed -i "s/\-${BTCVER[1]}\(\-dirty\)\?//g" "$TOPDIR/doc/cli/$cmdname.md"
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o "$TOPDIR/doc/man/$cmdname.1" "$cmd"
  sed -i "s/\\\-${BTCVER[1]}\(\\\-dirty\)\?//g" "$TOPDIR/doc/man/$cmdname.1"
done

rm -f footer.h2m
