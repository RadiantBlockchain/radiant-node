macOS Build Instructions and Notes
====================================

The commands in this guide should be executed in a Terminal application.
The built-in one is located in `/Applications/Utilities/Terminal.app`.

Preparation
-----------

Install [Homebrew](https://brew.sh), and install dependencies:

```
brew install berkeley-db boost cmake libevent librsvg miniupnpc ninja openssl protobuf python qrencode qt5 zeromq help2man
```

You can do without the `miniupnpc`, `zeromq`, and `help2man` packages, then you
just need to respectively pass `-DENABLE_UPNP=OFF`, `-DBUILD_RADIANT_ZMQ=OFF`,
or `-DENABLE_MAN=OFF` on the `cmake` command line.

You can do without the `librsvg`, `qrencode` and `qt5` packages, if you don't
intend to build the GUI.

You can do without the `berkeley-db` package, if you don't intend to build
the wallet.

See [dependencies.md](dependencies.md) for a complete overview.

If you intend to build the GUI, you also need to install
[Xcode](https://apps.apple.com/us/app/xcode/id497799835) from the App
Store (it's a dependency for qt5). Install the macOS command line tools:

```
xcode-select --install
```

When the popup appears, click *Install*.

If you want to run ZMQ tests with the test framework, you need the zmq python module:

```
pip3 install pyzmq
```

Build Radiant Node
------------------------

Before you start building, please make sure that your compiler supports C++17.

Clone the Radiant Node source code and cd into `radiant-node`

```
git clone https://github.com/radiantblockchain/radiant-node.git
cd radiant-node
```

Create a build directory to build out-of-tree.

```
mkdir build
cd build
```

Configure and build the headless Radiant Node binaries, as well as the GUI.

You can disable the GUI build by passing `-DBUILD_RADIANT_QT=OFF` to `cmake`.

```
cmake -GNinja ..
ninja
```

Finally it is recommended to build and run the unit tests:

```
ninja check
```

You can create a .dmg that contains the .app bundle (optional):

```
ninja osx-dmg
```

After building the Radiant Node binaries are available
at `./src/radiantd`. You can install to the system with

```
sudo ninja install
```

Disable-wallet mode
--------------------

When the intention is to run only a P2P node without a wallet, Radiant Node
may be compiled in disable-wallet mode with:

```
cmake -GNinja .. -DBUILD_RADIANT_WALLET=OFF -DBUILD_RADIANT_QT=OFF
ninja
ninja check
```

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

Running
-------

Before running, it's recommended that you create an RPC configuration file:

```
echo -e "rpcuser=radiantrpc\nrpcpassword=$(xxd -l 16 -p /dev/urandom)" > "/Users/${USER}/Library/Application Support/Radiant/radiant.conf"
chmod 600 "/Users/${USER}/Library/Application Support/Radiant/radiant.conf"
```

The first time you run `radiantd` or the GUI, it will start downloading the blockchain.
This process could take many hours, or even days on slower than average systems.

You can monitor the download process by looking at the debug.log file:

```
tail -f $HOME/Library/Application\ Support/Radiant/debug.log
```

Other commands
--------------

```
radiantd -daemon # Starts the radiant daemon.
radiant-cli --help # Outputs a list of command-line options.
radiant-cli help # Outputs a list of RPC commands when the daemon is running.
```

Notes
-----

* Building with downloaded Qt binaries is not officially supported. See the
  notes in [#7714](https://github.com/radiant/radiant/issues/7714)
