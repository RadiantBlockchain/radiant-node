Release Notes for Bitcoin Cash Node version 0.21.3
==================================================

Bitcoin Cash Node version 0.21.3 is now available from:

  <https://bitcoincashnode.org>


Overview
--------

...


Usage recommendations
---------------------

The update to Bitcoin Cash Node 0.21.3 is optional.

We recommend Bitcoin Cash Node 0.21.3 as a replacement for
Bitcoin ABC 0.21.x

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


Note regarding BIP9 and `getblockchaininfo`
-------------------------------------------

Bitcoin Cash Node 0.21.3 removed the (incomplete) BIP9 support. In earlier
versions, it already was inactive due to no available proposals to vote on. The
empty `softforks` field in `getblockchaininfo` will be removed in version 0.22.


Deprecation note regarding `medianfeerate` field in `getblockstats`
-----------------------------------------------------------------------

The `medianfeerate` field in `getblockstats` output is deprecated, will be removed
in v0.22. The 50th percentile from 'feerate_percentiles' array should be used instead.

Deprecation note regarding the autotools build system
-----------------------------------------------------

The autotools build system (`autogen`, `configure`, ...) is deprecated and
will be removed in a future release. CMake is the replacement build system,
look at the documentation for the build instructions. To continue using the
autotools build system, pass the --enable-deprecated-build-system flag to
`configure`.

New RPC methods
---------------

...


Low-level RPC changes
----------------------

...


Regressions
-----------

Bitcoin Cash Node 0.21.3 does not introduce any known regressions compared
to 0.21.2.


Known Issues
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

**New documents:**

...


**Removed documents:**

...


**Notable commits grouped by functionality:**

Security or consensus relevant fixes

...


Interfaces / RPC

...


Peformance optimizations

...


GUI

...


Code quality

...


Documentation updates

...


Build / general

...


Build / Linux

...


Build / MacOSX

...


Tests / test framework

...


Benchmarks

...


Seeds / seeder software

...


Maintainer tools

...


Infrastructure

...


Cleanup

...


Continuous Integration (GitLab CI)

...

Backports

...

