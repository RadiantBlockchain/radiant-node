# Ubuntu & Debian build guide

Updated for Ubuntu 19.04 and Debian Buster (10). If you run an older version,
please see [section below](build-unix-deb.md#getting-a-newer-cmake-on-older-os),
about obtaining the required version of `cmake`.

## Preparation

Minimal build requirements

```bash
    sudo apt-get install build-essential cmake git libboost-chrono-dev libboost-filesystem-dev libboost-test-dev libboost-thread-dev libevent-dev libminiupnpc-dev libssl-dev libzmq3-dev help2man ninja-build python3
```

You can do without the `libminiupnpc-dev`, `libzmq3-dev`, and `help2man`
packages, then you just need to respectively pass `-DENABLE_UPNP=OFF`,
`-DBUILD_RADIANT_ZMQ=OFF`, or `-DENABLE_MAN=OFF` on the `cmake` command line.

BerkeleyDB 5.3 or later is required for the wallet. This can be installed with:

```bash
    sudo apt-get install libdb-dev libdb++-dev
```

If you want to build the GUI client `bitcoin-qt` Qt 5 is necessary.
To build with Qt 5 you need the following packages installed:

```bash
    sudo apt-get install libqrencode-dev libprotobuf-dev protobuf-compiler qttools5-dev
```

You can do without the `libqrencode` package, just pass `-DENABLE_QRCODE=OFF`
on the cmake command line.

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

Next you need to choose between building just the node, the node with wallet
support, or the node and the QT client.

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

You will find the `bitcoind`, `bitcoin-cli`, `bitcoin-tx` (and optionally
`bitcoin-qt`) binaries in `/build/src/(qt)`.

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

## Getting a newer cmake on older OS

On versions prior to Ubuntu 19.04 and Debian 10, the `cmake` package is too old
and needs to be installed from the Kitware APT repository:

```bash
    sudo apt-get install apt-transport-https ca-certificates gnupg software-properties-common wget
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo apt-key add -
```

Add the repository corresponding to your version (see [instructions from Kitware](https://apt.kitware.com)).
For Ubuntu Bionic (18.04):

```bash
    sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
```

Then update the package list and install `cmake`:

```bash
    sudo apt update
    sudo apt install cmake
```

