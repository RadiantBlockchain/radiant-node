# Arch Linux build guide

## Preparation

You will need the following dependencies:

```bash
    pacman -S boost cmake git libevent ninja python miniupnpc zeromq help2man
```

You can do without the `miniupnpc`, `zeromq`, and `help2man` packages, then you
just need to respectively pass `-DENABLE_UPNP=OFF`, `-DBUILD_RADIANT_ZMQ=OFF`,
or `-DENABLE_MAN=OFF` on the `cmake` command line.

If you want to build the GUI client `bitcoin-qt` Qt 5 is necessary.
To build with Qt 5 you need the following packages installed:

```bash
pacman -S qt5-base qt5-tools protobuf qrencode
```

You can do without the `qrencode` package, just pass `-DENABLE_QRCODE=OFF` on
the `cmake` command line.

## Building

Once you have installed the required dependencies (see sections above), you can
build Radiant Node as such:

First fetch the code (if you haven't done so already).

```bash
git clone https://github.com/radiantblockchain/radiant-node.git
```

Change to the BCN directory, make `build` dir, and change to that directory

```bash
cd radiant-node/
mkdir build
cd build
```

Next you need to choose between building just the node, the node with wallet support,
or the node and the QT client.

**Choose one:**

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

You will find the `radiantd`, `radiant-cli`, `radiant-tx` (and optionally `radiant-qt`)
binaries in `/build/src/(qt)`.

Optionally, run the tests

```bash
ninja check # recommended
```

After a successful test you can install the newly built binaries to your `bin` directory.
Note that this will probably overwrite any previous version installed, including
binaries from different sources.

```bash
sudo ninja install #optional
```

