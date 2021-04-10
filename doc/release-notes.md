# Release Notes for Bitcoin Cash Node version 23.0.0

Bitcoin Cash Node version 23.0.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

...


## Usage recommendations

...


## Network changes

The `dsproof-beta` network message can be emitted and relayed
when Double Spend Proofs are enabled (which they are by default).


## Added functionality

### Double Spend Proofs (DSProof)

This release adds Double Spend Proofs that are compatible
with the published `dsproof-beta` message specification and
existing implementations in Bitcoin Unlimited and Flowee The Hub.

Double spend proofs are enabled by default, but can be turned
off with the `doublespendproof=0` configuration setting.

This functionality is still in beta. In future BCHN releases
we plan to gradually add more application interfaces to query
double spend proof information about transactions and to provide
wallet user with double spend notifications.

Please refer to </doc/dsproof-implementation-notes.md> for more
information details on the DSProof implementation in BCHN.


## Deprecated functionality

In the `getmempoolentry` RPC call, the verbose modes of the
`getrawmempool`/`getmempoolancestors`/`getmempooldescendants` RPC calls, and the
JSON mode of the mempool REST call, the `height` field is deprecated and will be
removed in a subsequent release. This field indicates the block height upon
mempool acceptance.

...


## Removed functionality

Manpages are no longer available in the autotools build system. You must switch
to the CMake build system to continue using manpages. Note the autotools build
system has been deprecated since v22.0.0.

## New RPC methods

The 'validateblocktemplate' RPC call has been added.
This call checks if a block template would be accepted if the hash solution
were solved.
The semantics of this call are the same as on BCHUnlimited, from
where a substantial portion of this new functionality has been ported.
It is intended to be used by services that test compatibility of block
generation with BCHN's consensus rules.

## Low-level RPC changes

The `getblockstats` RPC is faster for fee calculation by using BlockUndo data.
Also, `-txindex` is no longer required and `getblockstats` works for all
non-pruned blocks.

...


## Regressions

Bitcoin Cash Node 23.0.0 does not introduce any known regressions compared
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
  
- In the `getmempoolentry` RPC call, the verbose modes of the
  `getrawmempool`/`getmempoolancestors`/`getmempooldescendants` RPC calls, and
  the JSON mode of the mempool REST call, the `height` field shows an incorrect
  block height after a node restart. Note the field is deprecated and will be
  removed in a subsequent release.

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

- TODO: double spend proof network message commits

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

