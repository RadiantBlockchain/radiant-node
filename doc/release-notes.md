# Release Notes for Bitcoin Cash Node version 22.3.0

Bitcoin Cash Node version 22.2.2 is now available from:

  <https://bitcoincashnode.org>

## Overview

...


## Usage recommendations

...


## Network changes

...


## Added functionality

...


## Deprecated functionality

...


## Removed functionality

...


## New RPC methods

...

## Low-level RPC changes

The `getblockstats` RPC is faster for fee calculation by using BlockUndo data.
Also, `-txindex` is no longer required and `getblockstats` works for all
non-pruned blocks.

## Low-level RPC changes

...


## Regressions

Bitcoin Cash Node 22.3.0 does not introduce any known regressions compared
to 22.2.0.


## Known Issues

Some issues could not be closed in time for release, but we are tracking
all of them on our GitLab repository.

- MacOS versions earlier than 10.12 are no longer supported. Additionally,
  Bitcoin Cash Node does not yet change appearance when macOS "dark mode"
  is activated.

- Windows users are recommended not to run multiple instances of bitcoin-qt
  or bitcoind on the same machine if the wallet feature is enabled.
  There is risk of data corruption if instances are configured to use the same
  wallet folder.

- Some users have encountered unit tests failures when running in WSL
  environments (e.g. WSL/Ubuntu).  At this time, WSL is not considered a
  supported environment for the software. This may change in future.

  The functional failure on WSL is tracked in Issue #33.
  It arises when competing node program instances are not prevented from
  opening the same wallet folder. Running multiple program instances with
  the same configured walletdir could potentially lead to data corruption.
  The failure has not been observed on other operating systems so far.

- `doc/dependencies.md` needs revision (Issue #65).

- `test_bitcoin` can collide with temporary files if used by more than
  one user on the same system simultaneously. (Issue #43)

- For users running from sources built with BerkeleyDB releases newer than
  the 5.3 which is used in this release, please take into consideration
  the database format compatibility issues described in Issue #34.
  When building from source it is recommended to use BerkeleyDB 5.3 as this
  avoids wallet database incompatibility issues with the official release.

- There is a documentation build bug that causes some ordered lists on
  docs.bitcoincashnode.org to be rendered incorrectly (Issue #141).

- The `test_bitcoin-qt` test executable fails on Linux Mint 20
  (see Issue #144). This does not otherwise appear to impact the functioning
  of the BCHN software on that platform.

- An 'autotools' build (the old build method) fails on OSX when using Clang.
  (Issue #129)

- With a certain combination of build flags that included disabling
  the QR code library, a build failure was observed where an erroneous
  linking against the QR code library (not present) was attempted (Issue #138).

- The 'autotools' build is known to require some workarounds in order to
  use the 'fuzzing' build feature (Issue #127).

- Some functional tests are known to fail spuriously with varying probability.
  (see e.g. issue #148, and a fuller listing in #162).

- Possible out-of-memory error when starting bitcoind with high excessiveblocksize
  value (Issue #156)

- There are obstacles building the current version on Ubuntu 16.04 (see #187).
  These are worked around by our packaging system, but users trying to build
  the software from scratch are advised to either upgrade to more recent Ubuntu,
  or retrofit the necessary toolchains and perform the same build steps for
  Xenial as registered in our packaging repository, or build in a VM using
  the gitian build instructions, or run our reproducible binary release builds.

- The `getnetworkhashps` RPC call is not adapted to per-block DAA (see #193).
  It has an option to calculate average hashrate "since last difficulty change"
  and wrongly assumes difficulty changes every 2016 blocks. This irrelevant
  option will likely be removed in the next release.

- A problem was observed on scalenet where nodes would sometimes hang for
  around 10 minutes, accepting RPC connections but not responding to them
  (see #210).

- `arc lint` will advise that some `src/` files are in need of reformatting
  or contain errors. - this is because code style checking is currently a work in
  progress while we adjust it to our own project requirements (see Issue #75).
  One file in `doc` also violates the lint tool (Issue #153), and a new
  script in `test/benchmark/` likewise contains code that is flagged by
  the current linting configuration. There are also RPC parameter type
  inconsistencies that flag linting checks (see #182).

---

## Changes since Bitcoin Cash Node 22.2.0

### New documents

...

### Removed documents

...

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

...

#### Interfaces / RPC

...

#### Peformance optimizations

...

#### GUI

...

#### Code quality

...

#### Documentation updates

...

#### Build / general

...

#### Build / Linux

...

#### Build / Windows

...

#### Build / MacOSX

...

#### Tests / test framework

...

#### Benchmarks

...

#### Seeds / seeder software

...

#### Maintainer tools

...

#### Infrastructure

...

#### Cleanup

...

#### Continuous Integration (GitLab CI)

...

#### Backports

...

