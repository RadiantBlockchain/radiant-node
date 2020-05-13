#!/usr/bin/env bash
# Copyright (c) 2016-2019 The Bitcoin Core developers
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

export LC_ALL=C
TOPDIR=${TOPDIR:-$(git rev-parse --show-toplevel)}
BUILDDIR=${BUILDDIR:-$TOPDIR}

BINDIR=${BINDIR:-$BUILDDIR/src}
DOCDIR=${DOCDIR:-$TOPDIR/doc}

CONVERTOR=${CONVERTOR:-$TOPDIR/contrib/devtools/cli-help-to-markdown.py}

BITCOIND=${BITCOIND:-$BINDIR/bitcoind}
BITCOINCLI=${BITCOINCLI:-$BINDIR/bitcoin-cli}
BITCOINTX=${BITCOINTX:-$BINDIR/bitcoin-tx}
BITCOINQT=${BITCOINQT:-$BINDIR/qt/bitcoin-qt}
BITCOINSEEDER=${BITCOINSEEDER:-$BINDIR/seeder/bitcoin-seeder}

[ ! -x $BITCOIND ] && echo "$BITCOIND not found or not executable." && exit 1

# The autodetected version git tag can screw up manpage output a little bit
read -r -a BTCVER <<< "$($BITCOINCLI --version | head -n1 | awk -F'[ -]' '{ print $7, $8 }')"

# Create a footer file with copyright content.
# This gets autodetected fine for bitcoind if --version-string is not set,
# but has different outcomes for bitcoin-qt and bitcoin-cli.
echo "[COPYRIGHT]" > footer.h2m
$BITCOIND --version | sed -n '1!p' >> footer.h2m

for cmd in $BITCOIND $BITCOINQT; do
  cmdname="${cmd##*/}"
  ${CONVERTOR} "`${cmd} -?? -lang=en_US`" > ${DOCDIR}/cli/${cmdname}.md
  sed -i "s/\-${BTCVER[1]}//g" ${DOCDIR}/cli/${cmdname}.md
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${DOCDIR}/man/${cmdname}.1 --help-option="-? -lang=en_US" --version-option="-version -lang=en_US" ${cmd}
  sed -i "s/\\\-${BTCVER[1]}//g" ${DOCDIR}/man/${cmdname}.1
done

for cmd in $BITCOINCLI $BITCOINTX $BITCOINSEEDER; do
  cmdname="${cmd##*/}"
  ${CONVERTOR} "`${cmd} -?`" > ${DOCDIR}/cli/${cmdname}.md
  sed -i "s/\-${BTCVER[1]}//g" ${DOCDIR}/cli/${cmdname}.md
  help2man -N --version-string=${BTCVER[0]} --include=footer.h2m -o ${DOCDIR}/man/${cmdname}.1 ${cmd}
  sed -i "s/\\\-${BTCVER[1]}//g" ${DOCDIR}/man/${cmdname}.1
done

rm -f footer.h2m
