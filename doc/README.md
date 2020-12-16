Bitcoin Cash Node Setup
=======================

Bitcoin Cash Node is a fork of Bitcoin ABC, which is an original Bitcoin Cash
client and builds the backbone of the network. It downloads and, by default,
stores the entire history of Bitcoin Cash transactions, which requires a few
hundred gigabytes of disk space. Depending on the speed of your computer and
network connection, the synchronization process can take anywhere from a few
hours to a day or more.

To download Bitcoin Cash Node, visit [bitcoincashnode.org](https://bitcoincashnode.org/).

Verify
---------------------

If you download the associated signature files with the binaries from the above link,
you can verify the integrity of the binaries by following these instructions, replacing
VERSION with the value relevant to you:

Get the keys for versions 0.21.1 or later:

```
VERSION="0.21.1"
URL="https://download.bitcoincashnode.org/releases/${VERSION}/src/bitcoin-cash-node-${VERSION}.tar.gz"
KEYS_FILE="bitcoin-cash-node-${VERSION}/contrib/gitian-signing/keys.txt"
wget -q -O - "${URL}" | tar -zxOf - "${KEYS_FILE}" | while read FINGERPRINT _; do gpg --recv-keys "${FINGERPRINT}"; done
```

Get the keys for version 0.21.0:
```
URL="https://download.bitcoincashnode.org/keys/keys.txt"
wget -q -O - "${URL}" | while read FINGERPRINT _; do gpg --recv-keys "${FINGERPRINT}"; done
```

Check the binaries (all versions):
```
FILE_PATTERN="./*-sha256sums.${VERSION}.asc"
gpg --verify-files ${FILE_PATTERN}
grep "bitcoin-cash-node-${VERSION}" ${FILE_PATTERN} | cut -d " " -f 2- | xargs ls 2> /dev/null |\
  xargs -i grep -h "{}" ${FILE_PATTERN} | uniq | sha256sum -c
```

*IMPORTANT NOTE:* The first time you run this, all of the signing keys will be
UNTRUSTED and you will see warnings indicating this. For best security practices,
you should `gpg --sign-key <signer key>` for each release signer key and rerun
the above script (there should be no warnings the second time). If the keys change
unexpectedly, the presence of those warnings should be heeded with extreme caution.

Running
---------------------
The following are some helpful notes on how to run Bitcoin Cash Node on your
native platform.

### Unix

Unpack the files into a directory and run:

- `bin/bitcoin-qt` (GUI) or
- `bin/bitcoind` (headless)

### Windows

Unpack the files into a directory, and then run `bitcoin-qt.exe`.

### macOS

Drag `bitcoin-cash-node` to your applications folder, and then run `bitcoin-cash-node`.

### Need Help?

* See the documentation at the [Bitcoin Wiki](https://en.bitcoin.it/wiki/Main_Page)
  for help and more information.
* Ask for help on the [Bitcoin Cash Node Subreddit](https://www.reddit.com/r/bitcoincashnode/).

License
---------------------
Distribution is done under the [MIT software license](../COPYING).
This product includes software developed by the OpenSSL Project for use in the
[OpenSSL Toolkit](https://www.openssl.org/), cryptographic software written by
Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software
written by Thomas Bernard.
