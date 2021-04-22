# Release Notes for Bitcoin Cash Node version 23.0.1

Bitcoin Cash Node version 23.0.1 is now available from:

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

## Modified functionality

...

## Removed functionality

...

## New RPC methods

...

## Low-level RPC changes

The `getmempoolancestors` and `getmempooldescendants` RPC methods now
return a list of transactions that are sorted topologically (with parents
coming before children). Previously they were sorted by transaction id.

Mempool entries from the verbose versions of: `getrawmempool`, `getmempoolentry`,
`getmempoolancestors`, and `getmempooldescendants` which contain a `spentby`
key now have the transactions in the `spentby` list sorted topologically (with
parents coming before children). Previously this list was sorted by transaction
id.

## User interface changes

...

## Regressions

Bitcoin Cash Node 23.0.1 does not introduce any known regressions compared
to 23.0.0.

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
  There is ongoing work on the build system to address underlying issues,
  but the related code changes were not ready in time for the v23.0.0 release.

- A problem was observed on scalenet where nodes would sometimes hang for
  around 10 minutes, accepting RPC connections but not responding to them
  (see #210).

- In the `getmempoolentry` RPC call, the verbose modes of the
  `getrawmempool`/`getmempoolancestors`/`getmempooldescendants` RPC calls, and
  the JSON mode of the mempool REST call, the `height` field shows an incorrect
  block height after a node restart. Note the field is deprecated and will be
  removed in a subsequent release.

- On some platforms, the splash screen can be maximized, but it cannot be
  unmaximized again (see #255). This has only been observed on Mac OSX,
  not on Linux or Windows builds.

---

## Changes since Bitcoin Cash Node 23.0.0

### New documents

The following are new documents in the BCHN software repository:

...

### Removed documents

...

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

None.

#### Interfaces / RPC

None.

#### Performance optimizations

None.

#### GUI

None.

#### Code quality

None.

#### Documentation updates

None.

#### Build / general

None.

#### Build / Linux

None.

#### Build / Windows

None.

#### Build / MacOSX

None.

#### Tests / test framework

None.

#### Benchmarks

None.

#### Seeds / seeder software

None.

#### Maintainer tools

None.

#### Infrastructure

None

#### Cleanup

None.

#### Continuous Integration (GitLab CI)

None.

#### Backports

None.
