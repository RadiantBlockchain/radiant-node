# Radiant Node Setup

Radiant Node is a node and wallet implementation for the Radiant network.
It downloads and, by default, stores the entire history of Radiant
transactions, which requires a few hundred gigabytes of disk space. Depending on
the speed of your computer and network connection, the synchronization process
can take anywhere from a few hours to a day or more.

To download Radiant Node, visit [radiantblockchain.org](https://radiantblockchain.org/).

## Verify

If you download the associated signature files with the binaries from the above link,
you can verify the integrity of the binaries by following these instructions, replacing
VERSION with the value relevant to you:

Get the keys for versions 0.21.1 or later:

```
VERSION="0.21.1"
URL="https://download.radiantblockchain.org/releases/${VERSION}/src/radiant-node-${VERSION}.tar.gz"
KEYS_FILE="radiant-node-${VERSION}/contrib/gitian-signing/keys.txt"
wget -q -O - "${URL}" | tar -zxOf - "${KEYS_FILE}" | while read FINGERPRINT _; do gpg --recv-keys "${FINGERPRINT}"; done
```

Get the keys for version 0.21.0:

```
URL="https://download.radiantblockchain.org/keys/keys.txt"
wget -q -O - "${URL}" | while read FINGERPRINT _; do gpg --recv-keys "${FINGERPRINT}"; done
```

Check the binaries (all versions):

```
FILE_PATTERN="./*-sha256sums.${VERSION}.asc"
gpg --verify-files ${FILE_PATTERN}
grep "radiant-node-${VERSION}" ${FILE_PATTERN} | cut -d " " -f 2- | xargs ls 2> /dev/null |\
  xargs -i grep -h "{}" ${FILE_PATTERN} | uniq | sha256sum -c
```

*IMPORTANT NOTE:* The first time you run this, all of the signing keys will be
UNTRUSTED and you will see warnings indicating this. For best security practices,
you should `gpg --sign-key <signer key>` for each release signer key and rerun
the above script (there should be no warnings the second time). If the keys change
unexpectedly, the presence of those warnings should be heeded with extreme caution.

## Running

The following are some helpful notes on how to run Radiant Node on your
native platform.

### Unix

Quick Start (Build from Source)

```bash
# to build just the node, no wallet functionality, choose this:
cmake -GNinja .. -DBUILD_RADIANT_WALLET=OFF -DBUILD_RADIANT_QT=OFF
```

```bash
# to build the node, with wallet functionality, but without GUI, choose this:
cmake -GNinja .. -DBUILD_RADIANT_QT=OFF
```

```bash
# to build node and QT GUI client, choose this:
cmake -GNinja ..
```

Next, finish the build

```bash
ninja
```

Install:

```bash
ninja install
```

Unpack the files into a directory and run:

- `bin/bitcoin-qt` (GUI) or
- `bin/bitcoind` (headless)

### Windows

Unpack the files into a directory, and then run `bitcoin-qt.exe`.

### macOS

Drag `radiant-node` to your applications folder, and then run `radiant-node`.

## Help

- Ask for help on the [Radiant Node Subreddit](https://www.reddit.com/r/bitcoincashnode/).

