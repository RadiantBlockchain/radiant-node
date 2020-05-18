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

# Generate JSON-RPC API documentation.
# Start a regtest node in a temporary data directory.
mkdir -p "$TOPDIR/doc/json-rpc/tmp"
"$BITCOIND" -daemon -debuglogfile=0 -regtest -datadir="$TOPDIR/doc/json-rpc/tmp" -rpcuser=gen-manpages -rpcpassword=gen-manpages
# Create temporary new mkdocs file.
rm -f "$TOPDIR/mkdocs-tmp.yml"
# Remove any existing JSON-RPC documentation.
rm "$TOPDIR"/doc/json-rpc/*.md
# Get daemon version which will be included in footer.
version="$($BITCOIND -version | head -n1)"
# Iterate over the existing mkdocs file and locate the entry for json-rpc/README.md.
indentation=""
while IFS= read -r line; do
  if [ "${line: -19}" == " json-rpc/README.md" ]; then
    # json-rpc/README.md found; preserve it.
    echo "$line" >> "$TOPDIR/mkdocs-tmp.yml"
    indentation="${line%%-*}"
    # The list of RPC commands will be inserted into the new mkdocs file below the readme entry.
    # Get the list of RPC commands from the node and process it.
    echo "Bitcoin Cash Node JSON-RPC commands" > "$TOPDIR/doc/json-rpc/README.md"
    echo "===================================" >> "$TOPDIR/doc/json-rpc/README.md"
    "$BITCOINCLI" -rpcwait -regtest -datadir="$TOPDIR/doc/json-rpc/tmp" -rpcuser=gen-manpages -rpcpassword=gen-manpages help | while read -r helpline; do
      if [ -n "$helpline" ]; then
        if [ "${helpline:0:3}" == "== " ] && [ "${helpline: -3}" == " ==" ]; then
          # Found a category.
          category="${helpline:3:-3}"
          # Write category to new mkdocs file.
          echo "$indentation- $category:" >> "$TOPDIR/mkdocs-tmp.yml"
          # Write category to readme file.
          {
              echo
              echo "## $category"
              echo
          } >> "$TOPDIR/doc/json-rpc/README.md"
        else
          # Found a command.
          command=${helpline%% *}
          # Write command to new mkdocs file.
          echo "$indentation    - $command: json-rpc/$command.md" >> "$TOPDIR/mkdocs-tmp.yml"
          # Create command help page.
          "$TOPDIR/contrib/devtools/rpc-help-to-markdown.py" "$($BITCOINCLI -rpcwait -regtest -datadir="$TOPDIR/doc/json-rpc/tmp" -rpcuser=gen-manpages -rpcpassword=gen-manpages help $command)" > "$TOPDIR/doc/json-rpc/$command.md"
          {
              echo
              echo "***"
              echo
              echo "*$version*"
          } >> "$TOPDIR/doc/json-rpc/$command.md"
          sed -i "s/\-${BTCVER[1]}\(\-dirty\)\?//g" "$TOPDIR/doc/json-rpc/$command.md"
          # Write command to readme file.
          if [ "$command" == "$helpline" ]; then
            echo "* [**\`$command\`**]($command.md)" >> "$TOPDIR/doc/json-rpc/README.md"
          else
            echo "* [**\`$command\`**\` ${helpline:${#command}}\`]($command.md)" >> "$TOPDIR/doc/json-rpc/README.md"
          fi
        fi
      fi
    done
    {
        echo
        echo "***"
        echo
        echo "*$version*"
    } >> "$TOPDIR/doc/json-rpc/README.md"
    sed -i "s/\-${BTCVER[1]}\(\-dirty\)\?//g" "$TOPDIR/doc/json-rpc/README.md"
  else
    # Detect the end of indentation below the readme entry.
    if [ "${line:0:${#indentation}}" != "$indentation" ]; then
      indentation=""
    fi
    # Copy existing mkdocs entries into the new mkdocs file, but
    # skip all existing entries below the readme entry on the same indentation level.
    # This removes previously generated RPC documentation.
    if [ "$indentation" == "" ]; then
      echo "$line" >> "$TOPDIR/mkdocs-tmp.yml"
    fi
  fi
done < "$TOPDIR/mkdocs.yml"
# Stop the regtest node
"$BITCOINCLI" -rpcwait -regtest -datadir="$TOPDIR/doc/json-rpc/tmp" -rpcuser=gen-manpages -rpcpassword=gen-manpages stop
# Replace the old mkdocs file with the new one.
mv -f "$TOPDIR/mkdocs-tmp.yml" "$TOPDIR/mkdocs.yml"
# Remove the temporary node data directory
rm -r "$TOPDIR/doc/json-rpc/tmp"
