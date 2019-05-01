Release Notes for Bitcoin Cash Node version 22.0.0
==================================================

Bitcoin Cash Node version 22.0.0 is now available from:

  <https://bitcoincashnode.org>

Overview
--------

...


Usage recommendations
---------------------

The update to Bitcoin Cash Node 22.0.0 is required for the November 2020 Bitcoin Cash network upgrade.

MacOS versions earlier than 10.12 are no longer supported.
Additionally, Bitcoin Cash Node does not yet change appearance when macOS
"dark mode" is activated.

Windows users are recommended not to run multiple instances of bitcoin-qt
or bitcoind on the same machine if the wallet feature is enabled.
There is risk of data corruption if instances are configured to use the same
wallet folder.

Some users have encountered unit tests failures when running in WSL
environments (e.g. WSL/Ubuntu).  At this time, WSL is not considered a
supported environment for the software. This may change in future.


Semantic version numbering
--------------------------

As of version 22.0.0, Bitcoin Cash Node uses [Semantic Versioning](https://semver.org/) for its version numbers.
Hence our version numbers no longer start with a zero.


Note regarding BIP9 and `getblockchaininfo`
-------------------------------------------

Bitcoin Cash Node 22.0.0 removed the (incomplete) BIP9 support. In earlier
versions, it already was inactive due to no available proposals to vote on. The
empty `softforks` field in `getblockchaininfo` will be removed.


Deprecation note regarding `medianfeerate` field in `getblockstats`
-----------------------------------------------------------------------

The `medianfeerate` field in `getblockstats` output is deprecated, and will be
removed. The 50th percentile from 'feerate_percentiles' array should be used
instead.

Deprecation note regarding the autotools build system
-----------------------------------------------------

The autotools build system (`autogen`, `configure`, ...) is deprecated and
will be removed in a future release. CMake is the replacement build system,
look at the documentation for the build instructions. To continue using the
autotools build system, pass the --enable-deprecated-build-system flag to
`configure`.

Deprecation note regarding Windows 32-bit build
-----------------------------------------------

Support for 32-bit Windows builds will cease after BCHN version 22.0.0,
and build support for 32-bit Windows will be removed.

Regtest network now requires standard transactions by default
-------------------------------------------------------------

The regression test chain, that can be enabled by the `-regtest` command line
flag, now requires transactions to not violate standard policy by default.
Making the default the same as for mainnet, makes it easier to test mainnet
behavior on regtest. Be reminded that the testnet still allows non-standard
txs by default and that the policy can be locally adjusted with the
`-acceptnonstdtxn` command line flag for both test chains.

Changes to automatic banning
----------------------------

Automatic banning of peers for bad behavior has been slightly altered:

- Automatic bans are now referred to as "discouraged" in log output, as
  they're not (and weren't even before) strictly banned: incoming connections
  are still allowed from them (as was the case before this change), but they're
  preferred for eviction.
- Automatic bans will no longer time-out automatically after 24 hours.
  Depending on traffic from other peers, automatic bans may time-out at an
  indeterminate time.
- Automatic bans will no longer be persisted across restarts. Only manual bans
  will be persisted.
- Automatic bans will no longer be returned by the `listbanned` RPC.
- Automatic bans can no longer be lifted with the `setban remove` RPC command.
  If you need to remove an automatic ban, you must clear all automatic bans with
  the `clearbanned false true` RPC command, or restart the software to clear
  automatic bans.
- All extant automatic bans ("node misbehaving") that are currently stored in the
  node's `banlist.dat` file will be converted into "manual bans" and will expire
  within 24 hours after first running this version of BCHN.

CashAddr enabled by default in bitcoin-tx
-----------------------------------------

The bitcoin-tx tool has fully supported CashAddr since v0.21.2. CashAddr in JSON
output was disabled by default, but relying on this default was deprecated.
Version 22.0.0 now changes the default to enabled. Specify `-usecashaddr=0` to
retain the old behavior.

New RPC methods
---------------

...


Low-level RPC changes
----------------------

- The `clearbanned` method now optionally can take two additional boolean
  arguments (both default to true if unspecified). These arguments can be used
  to control whether manual or automatic bans are to be cleared (or both).
- The `listbanned` method no longer lists automatic bans.
- The `listbanned` method's results array has changed slightly. All entries
  now have their `banned_reason` as "manually added" (since `listbanned` can
  now only ever show manual bans). The "node misbehaving" value for this key
  will never appear. This key is now deprecated and may be removed altogether
  in a future release of BCHN.
- The `setban` method can no longer lift individual automatic bans. Use
  `clearbanned` instead to clear all bans, or `clearbanned false true` to
  clear all automatic bans (while preserving all manual bans).


Regressions
-----------

Bitcoin Cash Node 22.0.0 does not introduce any known regressions compared
to 0.21.2.


Known Issues
------------

...

Configuration
------------

Some issues could not be closed in time for release, but we are tracking
all of them on our GitLab repository.

- `doc/bips.md` needs revision (to be fixed in Issue #68).

- `doc/dependencies.md` needs revision (to be fixed in Issue #65).

- `arc lint` will advise that some `src/` files are in need of reformatting or
  contain errors - this is because code style checking is currently a work in
  progress while we adjust it to our own project requirements (see Issue #75).

- `test_bitcoin` can collide with temporary files if used by more than
  one user on the same system simultaneously. (Issue #43)

- We discovered a functional failure in Windows environment under a specific
  condition in Issue #33.
  It arises when competing node program instances are not prevented from
  opening the same wallet folder. Running multiple program instances with
  the same configured walletdir could potentially lead to data corruption.
  The failure has not been observed on other operating systems so far.

- For users running from sources built with BerkeleyDB releases newer than
  the 5.3 which is used in this release, please take into consideration
  the database format compatibility issues described in Issue #34.
  When building from source it is recommended to use BerkeleyDB 5.3 as this
  avoids wallet database incompatibility issues with the official release.

- BCHN project is currently considering improvements to specification and
  RPC outputs related to the SigOps -> SigChecks change that took effect
  on 15 May 2020. (ref. Issues #71, #72)


---

Changes since Bitcoin Cash Node 0.21.2
--------------------------------------

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


#### Build / MacOSX

- [> 0.21.3:] The 32 bits Windows target is no longer supported and has
  been removed from the release shipment.
  Users that wish to build for 32 bits Windows should be aware that
  this will not be tested by the Bitcoin Cash Node team and be prepared to
  resolve issues on their own.


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

