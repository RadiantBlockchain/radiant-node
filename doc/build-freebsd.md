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

If not installed, ZeroMQ support should be disabled by passing `-DBUILD_RADIANT_ZMQ=OFF`
to `cmake`.

In order to run the test suite (recommended), you will need to have Python 3:

```bash
pkg install python3
```

To run the ZeroMQ tests:

```bash
pkg install py37-pyzmq
```

For the wallet (optional):

```bash
pkg install db5
```

If you also want to build the GUI client `bitcoin-qt` Qt 5 is necessary.
To build with Qt 5 you need the following packages installed:

```bash
pkg install qt5-qmake qt5-buildtools qt5-linguisttools qt5-widgets protobuf qt5-testlib libqrencode-4.0.0
```

You can do without the `libqrencode-4.0.0` package, just pass `-DENABLE_QRCODE=OFF`
on the `cmake` command line.

To enable manpages:

```bash
pkg install help2man
```

If not installed, manpage generation should be disabled by passing `-DENABLE_MAN=OFF`
to `cmake`.

## Building Radiant Node

Download the source code:

```bash
git clone https://github.com/radiantblockchain/radiant-node.git
cd radiant-node/
```

To build with wallet:

```bash
mkdir build
cd build
cmake -GNinja -DBUILD_RADIANT_QT=OFF ..
ninja
ninja check # recommended
```

To build without wallet:

```bash
mkdir build
cd build
cmake -GNinja -DBUILD_RADIANT_QT=OFF -DBUILD_RADIANT_WALLET=OFF ..
ninja
ninja check # recommended
```

To build with wallet and GUI:

```bash
mkdir build
cd build
cmake -GNinja ..
ninja
ninja check # recommended
ninja test_bitcoin-qt # recommended
```

After a successful test you can install the newly built binaries to your bin directory.
Note that this will probably overwrite any previous version installed, including
binaries from different sources.
It might be necessary to run as root, depending on your system configuration:

```bash
ninja install #optional
```
