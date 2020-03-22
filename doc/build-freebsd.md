# FreeBSD build guide

(updated for FreeBSD 12.1)

This guide describes how to build bitcoind and command-line utilities on FreeBSD.

This guide does not contain instructions for building the GUI.

## Preparation

You will need the following dependencies, which can be installed as root via pkg:

```bash
pkg install cmake libevent ninja openssl boost-libs git
```

### Optional libraries

To enable UPnP:

```bash
pkg install miniupnpc
```

If not installed, UPnP support should be disabled by passing
`-DENABLE_UPNP=OFF` to `cmake`.

To enable ZeroMQ:

```bash
pkg install libzmq4
```

If not installed, ZeroMQ support should be disabled by passing `-DBUILD_BITCOIN_ZMQ=OFF`
to `cmake`.

In order to run the test suite (recommended), you will need to have Python 3 installed:

```shell
pkg install python3
```

To run the ZeroMQ tests:
```shell
pkg install py37-pyzmq
```
For the wallet (optional):

```bash
pkg install db5
```

## Building Bitcoin Cash Node

Download the source code:

```bash
git clone https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node.git
cd bitcoin-cash-node/
```

To build with wallet:

```bash
mkdir build
cd build
cmake -GNinja -DBUILD_BITCOIN_QT=OFF ..
ninja
ninja check # recommended
```

To build without wallet:

```bash
mkdir build
cd build
cmake -GNinja -DBUILD_BITCOIN_QT=OFF -DBUILD_BITCOIN_WALLET=OFF ..
ninja
ninja check # recommended
```

After a successful test you can install the newly built binaries to your bin directory.
Note that this will probably overwrite any previous version installed, including binaries from different sources.
It might be necessary to run as root, depending on your system configuration:

```bash
ninja install #optional
```
