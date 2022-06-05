# WINDOWS BUILD NOTES

Below are some notes on how to build Radiant Node for Windows.

Please note that from RADN v0.21.3 onwards, building for Win32 is no longer
officially supported (and build system capabilities related to this may
be removed).

The options known to work for building Radiant Node on Windows are:

- On Linux, using the [Mingw-w64](https://www.mingw-w64.org/downloads/) cross compiler
  tool chain. Debian Buster is recommended and is the platform used to build the
  Radiant Node Windows release binaries.
- On Windows, using [Windows Subsystem for Linux (WSL)](https://msdn.microsoft.com/commandline/wsl/about)
  and the Mingw-w64 cross compiler tool chain. This is covered in these notes.

Other options which may work, but which have not been extensively tested are
(please contribute instructions):

- On Windows, using a POSIX compatibility layer application such as
  [cygwin](http://www.cygwin.com/) or [msys2](http://www.msys2.org/).
- On Windows, using a native compiler tool chain such as
  [Visual Studio](https://www.visualstudio.com).

In any case please make sure that the compiler supports C++17.

**Note** These notes cover building binaries from source, for running Bitcoin
Cash Node natively under Windows. If you just want to run Radiant Node,
you can download binaries from the [Radiant Node website](https://radiantblockchain.org/en/download.html).
If you wish to both compile and run Radiant Node on Windows, *under WSL*,
you can refer to the [Unix build guide](build-unix.md),
and follow those instructions from within WSL.

## Windows Subsystem for Linux

With Windows 10, Microsoft has released a feature named the [Windows
Subsystem for Linux (WSL)](https://msdn.microsoft.com/commandline/wsl/about). This
feature allows you to run a bash shell directly on Windows in an Ubuntu-based
environment. Within this environment you can cross compile for Windows without
the need for a separate Linux VM or server. Note that while WSL can be installed
with other Linux variants, such as OpenSUSE, the following instructions have only
been tested with Ubuntu 20.04 (and 18.04, see below).

In May 2020 WSL 2 was released with Windows 10, Version 2004, Build 19041.

WSL is not supported in versions of Windows prior to Windows 10 or on
Windows Server SKUs. In addition, it is available only for 64-bit versions of
Windows.

## Building with WSL 2 and Ubuntu 20.04

This is the recommended method.

### Installing Ubuntu 20.04 on Windows Subsystem for Linux 2

It is beyond the scope of this guide to cover installation of WSL 2 and Ubuntu
20.04 on WSL 2. Instructions to install WSL 2 are available at the
[Windows Subsystem for Linux Installation Guide for Windows 10](https://docs.microsoft.com/en-us/windows/wsl/install-win10).

Once WSL 2 is installed Ubuntu 20.04 can be found in the
[Microsoft Store](https://www.microsoft.com/store/apps/9n6svws3rx71). You will
be asked to create a new UNIX user account. This is a separate account from your
Windows account.

Once the bash shell is active, you can log in and follow the instructions below,
starting with the "Cross-compilation" section. Compiling the 64-bit version is
recommended, but it is possible to compile the 32-bit version.

### Cross-compilation for Ubuntu and Windows Subsystem for Linux 2

The steps below can be performed on Ubuntu (including in a VM) or WSL 2. The depends
system will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

First, install the general dependencies:

```bash
    sudo apt update
    sudo apt upgrade
    sudo apt install autoconf automake build-essential bsdmainutils cmake curl git libboost-all-dev libevent-dev libssl-dev libtool ninja-build nsis pkg-config python3
```

A host toolchain (`build-essential`) is necessary because some dependency
packages (such as `protobuf`) need to build host utilities that are used in the
build process.

See also: [dependencies.md](dependencies.md).

### Building for 64-bit Windows

The first step is to install the `mingw-w64` cross-compilation tool chain.

```bash
    sudo apt install g++-mingw-w64-x86-64
```

Next, configure the `mingw-w64` to the posix[¹](#footnote1) compiler option.

```bash
    sudo update-alternatives --config x86_64-w64-mingw32-g++ # Set the default mingw32 g++ compiler option to posix.
    sudo update-alternatives --config x86_64-w64-mingw32-gcc # Set the default mingw32 gcc compiler option to posix.
```

Note that for WSL 2 the Radiant Node source path MUST be somewhere in the default
mount file system, for example `/usr/src/radiant-node`, AND not under `/mnt/d/`.
This means you cannot use a directory that is located directly on the host Windows
file system to perform the build.

Acquire the source in the usual way:

```bash
    git clone https://github.com/radiantblockchain/radiant-node.git
    cd radiant-node
```

Once the source code is ready the build steps are below:

```bash
    export PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') # strip out problematic Windows %PATH% imported var
    cd depends
    make build-win64
    cd ..
    mkdir build
    cd build
    cmake -GNinja .. -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/Win64.cmake -DENABLE_MAN=OFF -DBUILD_BITCOIN_SEEDER=OFF # seeder not supported in Windows yet
    ninja
    ninja package #to build the install-package
```

### Installation

After building using the Windows subsystem it can be useful to copy the compiled
executables to a directory on the windows drive in the same directory structure
as they appear in the release `.zip` archive. This can be done in the following
way. This will install to `c:\workspace\radiant-node`, for example:

```bash
    cmake -GNinja .. -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/Win64.cmake -DENABLE_MAN=OFF -DBUILD_BITCOIN_SEEDER=OFF -DCMAKE_INSTALL_PREFIX=/mnt/c/workspace/radiant-node
    sudo ninja install
```

## Building with WSL and Ubuntu 18.04

Building with WSL 2 and Ubuntu 20.04 is strongly recommended, but if for some
reason you find your self unable to install WSL 2, the below guide will work for
WSL. Here you will need to use Ubuntu 18.04, and that brings with it some extra
complications.

### Installing Ubuntu 18.04 on WSL

At the time of writing (April 2020) the Windows Subsystem for Linux installs Ubuntu
Focal 20.04 if you just search for Ubuntu in the Microsoft Shop. However, there is
a problem with the sleep function on WSL, so Ubuntu Bionic 18.04 is recomended, and
will be covered in these notes. See [this issue](https://github.com/microsoft/WSL/issues/4898)
for further info.

To install Ubuntu 18.04 on your WSL, you need to:

1. Install Ubuntu 18.04
    - Open Microsoft Store and search for Ubuntu 18.04 or use [this link](https://www.microsoft.com/store/productId/9n9tngvndl3q)
    - Click **Get**
2. Complete Installation
    - Open a cmd prompt and type "Ubuntu"
    - Create a new UNIX user account
      (this is a separate account from your Windows account)

After the bash shell is active, you can follow the instructions below, starting
with the "Cross-compilation" section. Compiling the 64-bit version is
recommended, but it is possible to compile the 32-bit version.

### Cross-compilation for Ubuntu and Windows Subsystem for Linux

The steps below can be performed on Ubuntu (including in a VM) or WSL. The depends
system will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

First, install the general dependencies:

```bash
    sudo apt update
    sudo apt upgrade
    sudo apt install autoconf automake build-essential bsdmainutils curl git libboost-all-dev libevent-dev libssl-dev libtool ninja-build pkg-config python3
```

The `cmake` version packaged with Ubuntu Bionic is too old for building Building
Radiant Node. To install the latest version:

```bash
    sudo apt-get install apt-transport-https ca-certificates gnupg software-properties-common wget
    wget -O - https://apt.kitware.com/keys/kitware-archive-latest.asc 2>/dev/null | sudo apt-key add -
    sudo apt-add-repository 'deb https://apt.kitware.com/ubuntu/ bionic main'
    sudo apt update
    sudo apt install cmake
```

A host toolchain (`build-essential`) is necessary because some dependency
packages (such as `protobuf`) need to build host utilities that are used in the
build process.

See also: [dependencies.md](dependencies.md).

### Building for 64-bit Windows

The first step is to install the `mingw-w64` cross-compilation tool chain.

```bash
    sudo apt install g++-mingw-w64-x86-64
```

Next, configure the `mingw-w64` to the posix[¹](#footnote1) compiler option.

```bash
    sudo update-alternatives --config x86_64-w64-mingw32-g++ # Set the default mingw32 g++ compiler option to posix.
    sudo update-alternatives --config x86_64-w64-mingw32-gcc # Set the default mingw32 gcc compiler option to posix.
```

Note that for WSL the Radiant Node source path MUST be somewhere in the default
mount file system, for example `/usr/src/radiant-node`, AND not under `/mnt/d/`.
This means you cannot use a directory that is located directly on the host Windows
file system to perform the build.

Acquire the source in the usual way:

```bash
    git clone https://github.com/radiantblockchain/radiant-node.git
    cd radiant-node
```

Once the source code is ready the build steps are below:

```bash
    export PATH=$(echo "$PATH" | sed -e 's/:\/mnt.*//g') # strip out problematic Windows %PATH% imported var
    cd depends
    make build-win64
    cd ..
    mkdir build
    cd build
    cmake -GNinja .. -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/Win64.cmake -DENABLE_MAN=OFF -DBUILD_BITCOIN_SEEDER=OFF # seeder not supported in Windows yet
    ninja
```

### Building RADN installer

To build a Windows installer for RADN you need a newer version of the `nsis` package
than is available in Ubuntu 18.04. To install a newer `nsis` from Ubuntu 19.10
Eoan you can do:

```bash
    sudo add-apt-repository "deb http://archive.ubuntu.com/ubuntu eoan universe"
    sudo apt install nsis
```

Then, back in the build directory, you can build the package with the command

```bash
    ninja package
```

### Installation

After building using the Windows subsystem it can be useful to copy the compiled
executables to a directory on the windows drive in the same directory structure
as they appear in the release `.zip` archive. This can be done in the following
way. This will install to `c:\workspace\radiant-node`, for example:

```bash
    cmake -GNinja .. -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/Win64.cmake -DENABLE_MAN=OFF -DBUILD_BITCOIN_SEEDER=OFF -DCMAKE_INSTALL_PREFIX=/mnt/c/workspace/radiant-node
    sudo ninja install
```

## Depends system

For further documentation on the depends system see [README.md](../depends/README.md)
in the depends directory.

## Footnotes

<a name="footnote1">1</a>: Starting from Ubuntu Xenial 16.04, both the 32 and 64
bit `Mingw-w64` packages install two different compiler options to allow a choice
between either posix or win32 threads. The default option is win32 threads which
is the more efficient since it will result in binary code that links directly with
the Windows kernel32.lib. Unfortunately, the headers required to support win32
threads conflict with some of the classes in the C++11 standard library, in particular
`std::mutex`. It's not possible to build the Radiant Node code using the win32
version of the Mingw-w64 cross compilers (at least not without modifying headers
in the Radiant Node source code).
