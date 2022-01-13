# Ninja build system targets

This document describes how the CMake/Ninja build system works and how to use it.

You will find some information on how to set up a Ninja build, and what are the
build targets and their effects.

## Generally understanding the Ninja build system

CMake is used to generate Ninja build files.

Please briefly acquaint yourself with the Ninja documentation at <https://ninja-build.org/manual.html>

## Setting up an out-of-tree Ninja build of Bitcoin Cash Node

An out-of-tree build creates build products in a separate folder where they do
not interfere with the source code working area.

An in-tree build is not recommended and therefore not described further, but it
is noted that in-tree builds are possible.

To set up an out-of-tree Ninja build:

1. create a build directory within your source tree:

    ```
    mkdir build/
    ```

2. enter the build folder:

    ```
    cd build/
    ```

3. configure the build:

    ```
    cmake -GNinja ..
    ```

    The command above will configure your build to build the application according
    to the feature dependencies you have installed.
    For more configuration options, see the final section in this document.

4. once step 3 has completed without errors, you are ready to call `ninja` to
   build various targets.

In the next sections, there is a description of the ninja build targets.
They are grouped according to their function.

For all these targets, run them by appending them to the `ninja` command.

## Cleanup-related targets

### `clean`

When used without further arguments, cleans up the build area by removing all
build products.

See also the description of the `clean` target at: <https://ninja-build.org/manual.html#_extra_tools>

If you need to 'clean up hardest' then the right way is to change to the
source folder of your checked out working area, and remove the entire
`build/` folder. (`rm -rf build`)

## Build-related targets

### (running ninja without arguments)

Running `ninja` without additional arguments will call the default target "all".

This builds the entire application based on the Cmake configuration
settings selected previously.

There are some test-related targets which are not automatically built
in this way.

Calling `ninja all` is equivalent to just running `ninja`.

### `check`

`ninja check` runs only basic unit tests on the daemon, the Qt wallet (if built),
and other utilities.

### `check-bitcoin`

Builds and runs only the core application's C++ unit tests.

### `check-bitcoin-upgrade-activated`

Similar to `check-bitcoin`, but runs the tests in a mode simulating that the next
upgrade has activated.

### `check-bitcoin-util`

Builds and runs application utility unit tests.

### `check-devtools`

Runs `devtools/chainparams/test_make_chainparams.py`.

This tests utilities to generate chainparams constants that are compiled into
the client.
See `contrib/devtools/chainparams/README.md` for more information on these tools.

### `check-lint`

Runs checking of our source code for programmatic and stylistic errors.

See [linting documentation](linting.md) for details.

### `security-check`

Runs a series of security tests for ELF binaries against the following executables:

- bitcoin-cli
- bitcoin-tx
- bitcoind
- bitcoin-qt
- bitcoin-seeder

If the checks are successful, the exit status of the script `security-check.py`
script will be 0 and its output will be silent, otherwise there will be a log
which executables failed which checks.

### `check-leveldb`

Build and run LevelDB test suite.

### `check-rpcauth`

Build and run Bitcoin RPC authentication tests.

### `check-secp256k1`

Build and run all secp256k1 tests.

### `check-secp256k1-tests`

Build and run only the 'non-exhaustive' secp256k1 tests (this is a subset of
`check-secp256k1` target).

### `check-secp256k1-exhaustive_tests`

Build and run only the 'exhaustive' secp256k1 tests (this is a subset of
`check-secp256k1` target).

### `check-all`

`ninja check-all` runs basic unit tests and the Python functional regression tests,
but not all (excludes so-called "extended" tests).

### `check-extended`

Runs unit tests and extended Python functional regression tests.

### `check-upgrade-activated`

Runs seeder and application unit and Python functional regression tests in a mode
where it simulates that the next planned upgrade (already built into the code)
has been activated.

### `check-upgrade-activated-extended`

Similar to check-upgrade-activated but with more extensive Python functional
regression tests (the `--extended` suite).

### `check-functional`

Builds the application (daemon and command line RPC client) and runs the Python-based
regression test suite, but only the basic (short/fast) regression tests, not the
extended ones.

### `check-functional-longeronly`

Runs only the longer-running Python tests by running the test_runner.py with
a `--startfrom=40` option (i.e. only run tests that have been timed previously
to take longer than 40 seconds).

### `check-functional-upgrade-activated`

Runs the Python regression tests with an option to simulate that thhe next upgrade
has been activated (currently `--with-phonon-activation` option).

### `check-functional-upgrade-activated-extended`

Similar to `check-functional-upgrade-activated` target, but runs with the `--extended`
option as well, so executing the full set of Python regression tests while simulating
that the next upgrade has been activated.

### `check-functional-upgrade-activated-longeronly`

Same as above but running long Python regression tests only (`--startfrom=40`).

### `coverage-<target>`

Generate coverage report for `<target>`. See [generate coverage report](coverage.md).

### `test_bitcoin`

Build the core application's C++ unit test runner (executable produced at
`src/test/test_bitcoin`)

### `test_bitcoin-qt`

Build the GUI wallet's C++ unit test runner (executable produced at
`src/qt/test/test_bitcoin-qt`)

### `test_bitcoin-seeder`

Build the DNS seeder's C++ unit test runner (executable produced at
`src/seeder/test/test_bitcoin-seeder`)

### `doc-html`

Build the [HTML documentation](publishing-documentation.md) in the `doc-html` folder.

### `doc-md`

Build the Markdown documentation in the `doc-md` folder and generate `mkdocs.yml`. The HTML documentation depends on this.

## Deployment / installation-related targets

### `install`

Builds and installs the project.

You can override the installation root (which defaults to /usr/local) by defining
the `CMAKE_INSTALL_PREFIX` when you configure your build, like this:

```
cmake -DCMAKE_INSTALL_PREFIX=${your_install_root_path} -GNinja ..
```

where you can use `$HOME` or `$(pwd)` (if you want to install into your build folder)
or whatever you prefer for `${your_install_root_path}`.

### `package`

Creates a versioned tarball package containing the executables, include header
files, libraries and manual pages.

### `package_source`

Creates a versioned tar.gz archive containing the project source code already
configured by cmake and ready to be built.

## Benchmarking targets

### `bench-bitcoin`

Builds and executes all application benchmarks. Excludes some library dependency
benchmarks (eg. secp256k1).

This target results in a comma separated output listing all benchmark names,
the number of iterations and timings associated with those (these will be machine-specific).

### `bench-secp256k1`

Build and run the secp256k1 benchmarks.
Outputs the benchmark information (names, timings).
Again, timings will be machine-specific.

## Fuzzing targets

### `bitcoin-fuzzers`

Builds application fuzz testing executables.

See [the fuzzing instructions](fuzzing.md) for more information.

## Other targets

### `asert-testgen`

Creates test vectors for ASERT difficulty adjustment algorithms. Requires
define `BUILD_ASERT_TEST_VECTORS=ON`

## Ninja build configuration options

Below are some of the cmake build options with example values.

All these options are passed via `-D` (defines) to the `cmake` command.

- -DBUILD_BITCOIN_SEEDER=OFF # to disable build of the seeder (it is not
  supported in Windows yet)

- -DCMAKE_INSTALL_PREFIX=/home/user   # set install prefix to user home directory

- -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/Win64.cmake  # select 64-bit Windows
  toolchain

- -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/LinuxARM.cmake  # select ARM Linux
  toolchain

- -DCMAKE_TOOLCHAIN_FILE=../cmake/platforms/LinuxAArch64.cmake # select AArch64
  Linux toolchain

- -DCCACHE=OFF  # disable ccache

- -DCMAKE_C_COMPILER=clang    # select clang as C compiler

- -DCMAKE_CXX_COMPILER=clang++    # select clang++ as C++ compiler

- -DCMAKE_C_COMPILER=afl-gcc    # select afl-gcc as C compiler

- -DCMAKE_CXX_COMPILER=afl-g++    # select afl-g++ as C++ compiler

- -DENABLE_GLIBC_BACK_COMPAT=ON

- -DENABLE_STATIC_LIBSTDCXX=ON

- -DEXCLUDE_FUNCTIONAL_TESTS # Skip specified functional tests for
  `check-functional` targets.

- -DENABLE_MAN=OFF # Disable manpage generation/installation

- -DDOC_ONLINE=ON # Adapt Markdown and HTML documentation for publication on docs.bitcoincashnode.org
