# UNIX BUILD NOTES

Some notes on how to build Radiant Node in Unix.

* For Ubuntu & Debian specific instructions, see [build-unix-deb.md](build-unix-deb.md)
  in this directory.
* For Fedora & CentOS specific instructions, see [build-unix-rpm.md](build-unix-rpm.md)
  in this directory.
* For Arch Linux specific instructions, see [build-unix-arch.md](build-unix-arch.md)
  in this directory.
* For Alpine specific instructions, see [build-unix-alpine.md](build-unix-alpine.md)
  in this directory.
* For FreeBSD specific instructions, see [build-freebsd.md](build-freebsd.md) in
  this directory.

## To Build

To build Radiant Node you first need to install all the needed dependencies.
See [dependencies.md](dependencies.md) or your OS specific guide in build-*.md.
If you wish to compile the dependencies from source yourself, please see
instructions in [depends](/depends/README.md).

Please make sure that your compiler supports C++17.

Assuming you have all the necessary dependencies installed the commands below will
build the node, with the `bitcoin-qt` GUI-client as well.

```bash
git clone https://github.com/radiantblockchain/radiant-node.git
cd radiant-node/
mkdir build
cd build
cmake -GNinja ..
ninja
ninja check # recommended
```

After a successful test you can install the newly built binaries to your bin directory.
Note that this will probably overwrite any previous version installed, including
binaries from different sources.
It might be necessary to run with `sudo`, depending on your system configuration:

```bash
ninja install #optional
```

### Disable-wallet mode

When the intention is to run only a P2P node without a wallet, Radiant Node
may be compiled in disable-wallet mode by passing `-DBUILD_RADIANT_WALLET=OFF`
on the `cmake` command line.

Mining is also possible in disable-wallet mode using the `getblocktemplate` RPC call.

### ARM Cross-compilation

These steps can be performed on, for example, a Debian VM. The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

Make sure you install all the build requirements mentioned above.
Then, install the toolchain and some additional dependencies:

```bash
sudo apt-get install autoconf automake curl g++-arm-linux-gnueabihf gcc-arm-linux-gnueabihf gperf pkg-config libtool
```

To build executables for ARM:

```bash
cd depends
make build-linux-arm
cd ..
mkdir build
cd build
cmake -GNinja .. -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/LinuxARM.cmake -DENABLE_GLIBC_BACK_COMPAT=ON -DENABLE_STATIC_LIBSTDCXX=ON
ninja
ninja check # recommended
```

For further documentation on the depends system see [README.md](../depends/README.md)
in the depends directory.

### AArch64 Cross-compilation

These steps can be performed on, for example, a Debian VM. The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

Make sure you install all the build requirements mentioned above.
Then, install the toolchain and some additional dependencies:

```bash
sudo apt-get install autoconf automake curl gcc-aarch64-linux-gnu g++-aarch64-linux-gnu gperf pkg-config libtool
```

To build executables for AArch64:

```bash
cd depends
make build-linux-aarch64
cd ..
mkdir build
cd build
cmake -GNinja .. -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/LinuxAArch64.cmake -DBUILD_RADIANT_ZMQ=OFF
ninja
```

For further documentation on the depends system see [README.md](../depends/README.md)
in the depends directory.

It's recommended that you run tests for your build. For testing the cross-compiled
binaries with an emulator, see section
*"Running functional tests in an emulator"*
in [functional-tests.md](functional-tests.md).

### Additional cmake options

A list of the cmake options and their current value can be displayed.
From the build subdirectory (see above), run `cmake -LH ..`.

### Memory Requirements

C++ compilers are memory-hungry. It is recommended to have at least 1.5 GB of
memory available when compiling Radiant Node. On systems with less, gcc can
be tuned to conserve memory with additional CXXFLAGS:

```bash
cmake -GNinja -DCXXFLAGS="--param ggc-min-expand=1 --param ggc-min-heapsize=32768" ..
```

### Strip debug symbols

The release is built with GCC and then `strip bitcoind` to strip the debug
symbols, which reduces the executable size by about 90%.

## miniupnpc

[miniupnpc](http://miniupnp.free.fr/) may be used for UPnP port mapping.
It can be downloaded from [here](http://miniupnp.tuxfamily.org/files/).
UPnP support is compiled in and turned off by default.
See the cmake options for upnp behavior desired:

```
ENABLE_UPNP            Enable UPnP support (miniupnp required, default ON)
START_WITH_UPNP        UPnP support turned on by default at runtime (default OFF)
```

## Security

To help make your Radiant Node installation more secure by making certain
attacks impossible to exploit even if a vulnerability is found, binaries are hardened
by default. This can be disabled by passing `-DENABLE_HARDENING=OFF`.

Hardening enables the following features:

* _Position Independent Executable_: Build position independent code to take
  advantage of Address Space Layout Randomization offered by some kernels. Attackers
  who can cause execution of code at an arbitrary memory location are thwarted if
  they don't know where anything useful is located.
  The stack and heap are randomly located by default, but this allows the code
  section to be randomly located as well.

    On an AMD64 processor where a library was not compiled with -fPIC, this will
    cause an error such as: "relocation R_X86_64_32 against `......' can not be
    used when making a shared object;"

    To test that you have built PIE executable, install `scanelf`, part of `pax-utils`,
    and use:

    ```bash
    scanelf -e ./bitcoin
    ```

    The output should contain:

    ```bash
    TYPE
    ET_DYN
    ```

* _Non-executable Stack_: If the stack is executable then trivial stack-based buffer
  overflow exploits are possible if vulnerable buffers are found. By default, Bitcoin
  Cash Node should be built with a non-executable stack, but if one of the libraries
  it uses asks for an executable stack or someone makes a mistake and uses a compiler
  extension which requires an executable stack, it will silently build an executable
  without the non-executable stack protection.

    To verify that the stack is non-executable after compiling use:

    ```bash
    scanelf -e ./bitcoin
    ```

    The output should contain:

    ```
    STK/REL/PTL
    RW- R-- RW-
    ```

    The `STK RW-` means that the stack is readable and writeable but not executable.

