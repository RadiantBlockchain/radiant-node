# Dependencies

## Usage

To build dependencies for the current architecture + OS:

```sh
make
```

To build for another architecture/OS, you may need to install
some required packages first. Please have a look at the section
"Install the required dependencies" below to check for your
target platform.

Once you have the required packages installed and done
any necessary preparation, run

```sh
make build-<platform>
```

Where `<platform>` is one of the following:

- linux64
- linux32
- linux-arm
- linux-aarch64
- osx
- win64

For example, building the dependencies for Linux on ARM:

```sh
make build-linux-arm
```

To use the dependencies for building Bitcoin Cash Node, you need to set
the platform file to be used by `cmake`.
The platform files are located under `cmake/platforms/`.
For example, cross-building for Linux on ARM (run from the project root):

```sh
mkdir build_arm
cd build_arm
cmake -GNinja .. -DENABLE_MAN=OFF -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/LinuxARM.cmake
ninja
```

Note that it will use all the CPU cores available on the machine by default.
This behavior can be changed by setting the `JOBS` environment variable (see
below).

For Mac OSX building, some preparatory steps are needed, including unpacking
the suitable SDK (how to obtain this SDK can be found in the `gitian-building.md`
document). Once the SDK has been obtained, the dependency building can be
done like this:

```sh
make download
mkdir SDKs
tar -C SDKs -xf /path/to/MacOSX10.15.sdk.tar.xz
make build-osx HOST=x86_64-apple-darwin16
```

Building the node against these dependencies is again a matter of (starting
from the project root):

```sh
mkdir build_osx
cd build_osx
cmake -GNinja .. -DENABLE_MAN=OFF -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/OSX.cmake
ninja
```

No other options are needed, the paths are automatically configured.

## Install the required dependencies: Ubuntu & Debian

### Common to all arch/OS

```sh
sudo apt-get install build-essential autoconf automake cmake curl git libtool ninja-build patch pkg-config python3 bison
```

### For macOS cross compilation

Install the following packages:

```sh
sudo apt-get install imagemagick libbz2-dev libcap-dev librsvg2-bin libtiff-tools python3-setuptools
```

Obtain the same SDK packge that is used in the gitian build (refer to `gitian-building.md` for download instructions).
Create an `SDKs` folder within the `depends` folder, if it does not already exist.
Unpack the SDK tarball there.

### For Win64 cross compilation

- see [build-windows.md](../doc/build-windows.md#cross-compilation-for-ubuntu-and-windows-subsystem-for-linux)

### For Linux cross compilation

Common linux dependencies:

```sh
sudo apt-get install gperf
```

For linux 32 bits cross compilation:

First add the i386 architecture to `dpkg`:

```sh
sudo dpkg --add-architecture i386
sudo apt-get update
```

Then install the dependencies:

```sh
sudo apt-get install lib32stdc++-8-dev libc6-dev:i386
```

For linux ARM cross compilation:

```sh
sudo apt-get install g++-arm-linux-gnueabihf
```

For linux AARCH64 cross compilation:

```sh
sudo apt-get install g++-aarch64-linux-gnu
```

## Dependency Options

The following can be set when running make: make FOO=bar

    SOURCES_PATH: downloaded sources will be placed here
    BASE_CACHE: built packages will be placed here
    SDK_PATH: Path where sdk's can be found (used by macOS)
    FALLBACK_DOWNLOAD_PATH: If a source file can't be fetched, try here before giving up
    NO_QT: Don't download/build/cache qt and its dependencies
    NO_ZMQ: Don't download/build/cache packages needed for enabling zeromq
    NO_WALLET: Don't download/build/cache libs needed to enable the wallet
    NO_UPNP: Don't download/build/cache packages needed for enabling upnp
    NO_JEMALLOC: Don't download/build/cache jemalloc
    DEBUG: disable some optimizations and enable more runtime checking
    RAPIDCHECK: build rapidcheck (experimental, requires cmake)
    NO_PROTOBUF: Don't download/build/cache protobuf (used for BIP70 support)
    HOST_ID_SALT: Optional salt to use when generating host package ids
    BUILD_ID_SALT: Optional salt to use when generating build package ids
    JOBS: Number of jobs to use for each package build

If some packages are not built, for example by building the depends with
`make NO_WALLET=1`, the appropriate options should be set when building Bitcoin
Cash Node using these dependencies.
In this example, `-DBUILD_BITCOIN_WALLET=OFF` should be passed to the `cmake`
command line to ensure that the build will not fail due to missing dependencies.

NOTE: The SDK_PATH should be set to the parent folder in which the
`MacOSX10.15.sdk/` is located. Alternatively, you can unpack the SDK within
the `depends/SDKs/` folder or create a symbolic link named `MacOSX10.15.sdk/`
to it from there.

Additional targets:

    download: run 'make download' to fetch all sources without building them
    download-osx: run 'make download-osx' to fetch all sources needed for macOS builds
    download-win: run 'make download-win' to fetch all sources needed for win builds
    download-linux: run 'make download-linux' to fetch all sources needed for linux builds
    build-all: build the dependencies for all the arch/OS

## Other documentation

- [description.md](description.md): General description of the depends system
- [packages.md](packages.md): Steps for adding packages
