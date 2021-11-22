# Release Notes for Bitcoin Cash Node version 24.0.1

Bitcoin Cash Node version 24.0.1 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a minor release of Bitcoin Cash Node (BCHN) that implements ...

This version contains further corrections and improvements, such as:

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

## User interface changes

...

## Regressions

Bitcoin Cash Node 24.0.1 does not introduce any known regressions as compared to 24.0.0.

## Known Issues

Some issues could not be closed in time for release, but we are tracking all of them on our GitLab repository.

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

- With a certain combination of build flags that included disabling
  the QR code library, a build failure was observed where an erroneous
  linking against the QR code library (not present) was attempted (Issue #138).

- Some functional tests are known to fail spuriously with varying probability.
  (see e.g. issue #148, and a fuller listing in #162).

- Possible out-of-memory error when starting bitcoind with high excessiveblocksize
  value (Issue #156)

- A problem was observed on scalenet where nodes would sometimes hang for
  around 10 minutes, accepting RPC connections but not responding to them
  (see #210).

- Startup and shutdown time of nodes on scalenet can be long (see Issue #313).

- On some platforms, the splash screen can be maximized, but it cannot be
  unmaximized again (see #255). This has only been observed on Mac OSX,
  not on Linux or Windows builds.

- There is an issue with `git-lfs` that may interfere with the refreshing
  of source code checkouts which have not been updated for a longer time
  (see Issues #326, #333). A known workaround is to do a fresh clone of the
  repository.

---

## Changes since Bitcoin Cash Node 24.0.0

### New documents

...

### Removed documents

...

### Notable commits grouped by functionality

...

#### Security or consensus relevant fixes

...

#### Interfaces / RPC

...

#### Performance optimizations

...

#### GUI

...

#### Data directory changes

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
