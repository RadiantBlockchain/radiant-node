Release Notes for Bitcoin Cash Node version 0.21.1
==================================================

Bitcoin Cash Node version 0.21.1 is now available from:

  <https://bitcoincashnode.org>


Overview
--------

This is a second, minor release of Bitcoin Cash Node before the May 2020
network upgrade. While it contains many corrections and improvements that
we feel are useful, it is not a required installation for the May 2020
network upgrade.

Bitcoin Cash Node is a drop-in replacement for Bitcoin ABC.
It is based on Bitcoin ABC 0.21.0, with minimal changes necessary to
disable the Infrastructure Funding Proposal (IFP) soft forks.
For exchanges and users, this client will follow the longest chain whether
it includes IFP soft forks or not. For miners, running this client ensures
the `getblocktemplate` RPC call will return a block with version bits that
vote "NO" for the IFP soft forks. Additionally, unlike Bitcoin ABC,
`getblocktemplate` will *not* automatically insert IFP white-list addresses
into the coinbase transaction.

This release contains one security fix and one correction which has been
identified as potentially relevant to consensus.

Security fix
------------

This release contains a patch for a denial-of-service security vulnerability
in [CVE-2019-18936 UniValue::read() in UniValue 1.0.3 and 1.0.4 allows a segfault via malformed JSON](https://github.com/bitcoin/bitcoin/issues/17742).

Attackers with limited JSON RPC access could exploit this vulnerability to
crash the node with a malicious JSON RPC instruction. Attackers with full
JSON RPC access do not need to exploit - they would be able to simply stop
a node. Attackers without JSON RPC access cannot exploit this vulnerability.
As a mitigation, untrusted inputs can be sanitised before sending them on
to the JSON RPC server. Note that the "malformed" JSON is technically valid,
just deeply nested. Upstream fix first released in Bitcoin Core 0.19.1.


Consensus-relevant fix
----------------------

The backport of D4936 from Bitcoin ABC ("update 'cousins' during UpdateFlags")
corrects a problem where some blocks incorrectly retained invalid markings when
`reconsiderblock` was called.

This is a not a consensus change that affects unattended operation, but
it can be relevant in certain operational and maintenance scenarios.


Usage recommendations
---------------------

We recommend Bitcoin Cash Node 0.21.1 as a drop-in replacement for
Bitcoin ABC 0.21.0.

The update from Bitcoin Cash Node 0.21.0 to 0.21.1 is optional.
Either version will work for May 2020 network upgrade.

Windows users are recommended not to run multiple instances of bitcoin-qt
or bitcoind on the same machine if the wallet feature is enabled.
There is risk of data corruption if instances are configured to use the same
wallet folder.

Some users have encountered unit tests failures when running in WSL
environments (e.g. WSL/Ubuntu).  At this time, WSL is not considered a
supported environment for the software. This may change in future.


Note regarding BIP9 and `getblockchaininfo`
-------------------------------------------

BIP9 is inactive due to no available proposals to vote on and it may be
removed in a future release.


Regressions
-----------

Bitcoin Cash Node 0.21.1 does not introduce any known regressions compared
to 0.21.0.


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

- A functional failure in Windows environment was observed in Issue #33.
  It arises when competing node program instances are not prevented from
  opening the same wallet folder. Running multiple program instances with
  the same configured walletdir could potentially lead to data corruption.
  The failure has not been observed on other operating systems so far.

- For users running from sources built with BerkeleyDB releases newer than
  the 5.3 which is used in this release, please take into consideration
  the database format compatibility issues described in Issue #34.
  When building from source it is recommended to use BerkeleyDB 5.3 as this
  avoids wallet database incompatibility issues with the official release.

- BCHN project is tracking some ongoing discussions related to details of the
  SigChecks specification and the output of getblocktemplate RPC call after
  the May upgrade (ref. Issues #71, #72). In the event that software changes
  become necessary we will advise users via our communication channels and
  inform of another minor release before May.

---

Changes since Bitcoin Cash Node 0.21.0
--------------------------------------

### New documents

- [xversionmessage.md](../xversionmessage.md) : (protocol) xversion / extversion draft specification
- [ninja_targets.md](../ninja_targets.md) : (development) describes build system targets
- [bchn-gitlab-usage-rules-and-guidelines.md](../bchn-gitlab-usage-rules-and-guidelines.md) : (process) rules for working on BCHN
- [release-notes/release-notes-0.21.0.md](release-notes-0.21.0.md) : previous version release notes
- [build-unix-arch.md](../build-unix-arch.md) : (build, refactor) split out from [build-unix.md](../build-unix.md)
- [build-unix-deb.md](../build-unix-deb.md) : (build, refactor) split out from [build-unix.md](../build-unix.md)
- [build-unix-rpm.md](../build-unix-rpm.md) : (build, refactor) split out from [build-unix.md](../build-unix.md)

### Removed documents

- gitian-building/gitian-building-mac-os-sdk.md : obsoleted instructions

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- 83252d0 [CVE-2019-18936] Pull UniValue subtree from Bitcoin Core
- 5a761db [validation.cpp] update 'cousins' during UpdateFlags (Bitcoin ABC)

#### Interfaces / RPC

- 0c019c5 Add help text for -parkdeepreorg and -automaticunparking
- 9d36ab1 Make hidden reorg protection RPC commands visible
- d0ed11f `getblocktemplate` RPC help fixes

#### Peformance optimizations

- 54d94d1 Don't park blocks when there is no actual reorg
- 4e9a173 The parked block marker, pindexBestParked, is set to null if it is about to be cleared.
- 3be2000 UniValue & RPC Interface: Significant performance improvements
- 472118a UniValue performance speedups for .write()

#### GUI

- 6ce344f Qt: Fix #47; quirk when switching versions after selecting Satoshi (sats)
- 9f1598b Qt: Set AA_EnableHighDpiScaling before QApplication instance creation
- a512311 Add 'GRAPHENE' and 'CF' GUI labels in nodes' supported services
- bbf89bb New splash screens (#29)
- cd18a55 Decent testnet/regtest logo/icon colours (#27)
- fc3ce07 Dark NSIS installer wizard image
- d0c078b Reorganise main window title (#38)
- b2bf997 Modify Bitcoin Qt desktop file to reflect we are Bitcoin Cash Node

#### Code quality

- 6786e42 Fix many PVS static analyzer warnings and/or errors
- 38e9fde13 Fix two compiler warnings (clang 11.0.0)
- 80e4cff sha256_shani.cpp + clang: suppress -Wcast-align warnings
- a40df80 [secp256k1] Remove a warning in multiset test
- 0bc6f69 Mute self assign warning in uint256_tests.cpp
- f7cdc60 [lint] Fix Python script linting transgressions
- 185b3a6 Restore the arcanist configs (revert of 767c5720)
- 6c139ae [CMAKE] Factorize the test suite target name construction

#### Documentation updates

- ff23d72 [doc] Add revised xversion spec (draft) - provided by Greg Griffith (Bitcoin Unlimited)
- 41f2204 d50e0f2 939f729 f67fa40 ae737a6 335cdc0 Update contributing, build, developer and gitian docs
- 86e229c 7dea808 Fix up obsolete ABC references
- aaf8a1d 313ef3a Add confirmed disclosure relationships (BCHD, Flowee, Knuth, Verde)
- 2b2deda 4e61089 Describe correct steps to build and run the benchmarks
- 90be3b5 961b7a3 Update FreeBSD instructions & add info to build bitcoin-qt
- d8129cc [doc] Add BCHN Gitlab development working rules & guidelines
- b1135cd Create a document to list and explain the Ninja targets for human consumption
- 0e500f6 Add info about where to just download the software.
- 9891620 [doc] Renaming from ABC in benchmarking.md
- 49cbb01 Add link to Bitcoin Cash Node Doxygen documentation
- 75da08c Update the backporting instructions
- a2ae3aa Fix UAHF references in dnsseed-policy.md
- 4ec61fb Fix help text dependency on number of cores (#23)
- 062f881 6edfae2 d3d4f4b 06586da Add release signing keys (freetrader, sickpig, Jt Freeman, Calin Culianu)
- f2deacb Add description new GitLab label: "needs-testing"
- f187c05d0 Update man pages as per release process

#### Build / general

- 6d3f9c6 fix inconsistency in benchmark binary name between ninja / autotools build
- 4493b2a [SECP256K1] Fix ability to compile tests without -DVERIFY.
- 245c7d14a Exclude gitian-building documents from Windows installer package (NSIS)

#### Build / Linux

- d559b10 Needed changes to get Ubuntu PPA services to buid deb packages

#### Build / MacOSX

- 5f22c42 Update to using MacOSX10.11.sdk.tar.xz from Github for gitian building

#### Tests / test framework

- 3d5b166 Add `startfrom` parameter to test_runner
- 6930a54 [QA] Add extra column in Python Test runner to show order in which tests were run
- 4f3fc89 Fix handling of functional test runner parameters --extended, --cutoff, and --startfrom
- 99db7fa Add test with Schnorr multisig flags to cover SCRIPT_VERIFY_NULLFAIL | SCRIPT_ENABLE_SCHNORR_MULTISIG
- 4530ec4 Fix overly specific exception which failed feature_block.py on FreeBSD 12.1
- 43b45cb Fix a race condition in abc-finalize-block
- 2653ef7 [backport] Add test to check that transactions expire from mempool

#### Benchmarks

- d8cf0f9 Bench: Added a more complex test to rpc_mempool.cpp
- 2ece470 Add an additional test, "JsonReadWrite1MBBlockData"
- dcd0859 Make BlockToJsonVerbose benchmark more challenging
- 822e0ff bench: Benchmark blockToJSON

#### Seeds / seeder software

- 419fa06fe update of static IP seeds
- 01e8356 Replace BTCfork DNS seeders which are moving to new domain names
- 254a6a4 Add unit tests for CSeederNode::ProcessMessage()

#### Maintainer tools

- c05d7074d Restore git-subtree-check.sh script
- fea33a6 Bring univalue back into the subtree list
- d76918e Move github-release to appropriate contrib sub-directory

#### Infrastructure

- dcf4337 Use BCHN download server as fallback for dependencies (depends/)

#### Cleanup

- dd7e62f Remove GitHub issue template; add Gitlab issue template 'Bug_report' in its place
- 5f1d605 Use consistent copyright notices
- c769b90 Remove declaration of undefined function accidentally introduced in D5135
- 4de32d7 Remove Teamcity leftovers
- 0e18d69 Remove SegWit references from RPC help
- c5bbcbd Remove BIP9 and BIP145 references from getblocktemplate RPC help

#### Continuous Integration (GitLab CI)

- 290de1a [ci] Add clang build
- 21e830e ac29dbe [ci] Add 1-eval benchmark execution to test stage
- b5546be Add static checks stage to CI pipeline
- 42a0f27 Add GitLab's code quality report job to test stage
- 5f98d13 [ci] junit test reporting
- c00421f [ci] fix functional test runner junit output
- a9472d8 [ci] Added jobs for remaining qa tests

#### Backports

- a693072 Core: #11269: [Mempool] CTxMemPoolEntry::UpdateAncestorState: modifySigOps param type
- e915394 Core: #12035: [qt] change µBTC to bits
- c929cde Core: #13264: [qt] Satoshi unit
- 39392f7 Core: #13424: Consistently validate txid / blockhash length and encoding in rpc calls
- 23d63d6 Core: #15114 Qt: Replace remaining 0 with nullptr
- 1b67393 Core: #16701 qt: Replace functions deprecated in Qt 5.13
- 7f5057f Core: #16706 Qt: Replace deprecated QSignalMapper with lambda slots
- 9149f81 Core: #16707 qt: Remove obsolete QModelIndex::child()
- 4570248 Core: #16708 qt: Replace obsolete functions of QSslSocket
- a4a992b Core: #16267 bench: Benchmark blockToJSON
- 3c69d86 Core: #16299 bench: Move generated data to a dedicated translation unit
- fc9e452 ABC: D5438 [Mempool] CTxMemPoolEntry::UpdateAncestorState: modifySigOps param type
- 8af632d ABC: D5432 [qt] Satoshi unit
- 597b5f7 ABC: D5385 qt: Remove obsolete QModelIndex::child()
- 99f028d ABC: D5382 qt: Replace obsolete functions of QSslSocket
- cdab28e ABC: D5370 Don't park blocks when there is no actual reorg
- 0146d4c ABC: D5369 qt: Replace functions deprecated in Qt 5.13
- a49ab88 ABC: D5341 Qt: Replace remaining 0 with nullptr
- 9d8cdf9 ABC: D5311 [CMAKE] Factorize the test suite target name construction
- 3ea4c52 ABC: D5310 Mute self assign warning in uint256_tests.cpp
- dd0bef5 ABC: D5309 [secp256k1] Remove a warning in multiset test
- 6381e8f ABC: D5307 [SECP256K1] Fix ability to compile tests without -DVERIFY
- 9e55fed ABC: D5304 Fix a race condition in abc-finalize-block
- e676256 ABC: D5296 [qt] change µBTC to bits
- d9948fa ABC: D5294 Consistently validate txid / blockhash length and encoding in rpc calls
- c994733 ABC: D5290 Move github-release to appropriate contrib sub-directory
- 34534d5 ABC: D4936 [validation.cpp] update 'cousins' during UpdateFlags
- d22fd1d ABC: D4436 Add unit tests for CSeederNode::ProcessMessage()
