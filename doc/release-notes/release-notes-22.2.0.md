# Release Notes for Bitcoin Cash Node version 22.2.0

Bitcoin Cash Node version 22.2.0 is now available from:

  <https://bitcoincashnode.org>

## Overview

This is a minor release of Bitcoin Cash Node that implements some
interface enhancements and includes a number of other corrections and
improvements.

Users who are running any of our previous releases are recommended to
upgrade to v22.2.0 as part of regular maintenance.


## Usage recommendations

Installing this version is optional, but we strongly recommend that users of
22.1.0 upgrade to this release for the corrections, performance improvements
and post-upgrade checkpoints contained in it.


## Network changes

Policy has been changed to enforce standard transactions on testnet4.
This is to have a test network that is close to mainnet.
If you wish to experiment with non-standard transactions, please use
testnet3 or scalenet.

This release reduces the interval between INV announcements (and
introduces a new command-line configurable option, -txbroadcastinterval,
with millisecond resolution. The default value to 500 ms for incoming
connections (and half that for outgoing) instead of 5 seconds incoming
(2 sec outgoing).

The rate at which new INV messages are broadcast has important
implications for scalability.

In order to mitigate spam attacks, the node will limit the rate at which
new transaction invs are broadcast to each peer. The default is to limit
this to 7 tx/sec per MB of excessive block size.
That is, with a 32 MB blocksize limit, no more than 224 tx INVs are allowed
to be broadcast each second. The broadcast rate limit is adjustable
through the -txbroadcastrate configuration option.


## Change of `-maxmempool` default value

The default value for `-maxmempool` has been changed to 10 times the
`excessiveblocksize`, i.e. 320 MB on mainet and 2560 MB on scalenet.
If your node does not have sufficient free memory to accomodate such a
maxmempool setting when running on `scalenet`, we recommend to upgrade your
memory capacity or override this default.


## Added functionality

### Configurable transaction broadcast interval and rate

Two new configuration have been added which control the INV broadcast
interval and rate, with millisecond resolution.

These options have important impact on user experience and scalability,
and have been configured with settings that improve on the previous
conditions.

For more information on these impact of these options, please refer to
the detailed description in:

<https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/merge_requests/746>

### Thread names in logs

Log lines can be prefixed with the name of the thread that caused the log. To
enable this behavior, use`-logthreadnames=1`.


## Deprecated functionality

The `getnetworkhashps` RPC has an option to calculate average hashrate
"since last difficulty change". This option works incorrectly, assuming
that difficulty changes every 2016 blocks. The option is considered
irrelevant with the per-block DAA adjustments introduced in 2017,
and is scheduled for removal in a future release.


## Removed functionality

No functionality is removed in this release.


## New RPC methods

No new RPC methods are added in this release,


## Low-level RPC changes

The `getblockstats` RPC is faster for fee calculation by using BlockUndo
data. Also, `-txindex` is no longer required and `getblockstats` works for
all non-pruned blocks.

A discrepancy was noticed in the argument and result value names of
`getexcessiveblock` and `setexcessiveblock` RPC calls between the interface
documentation, actual required argument names, error message and name of
returned result. A two-step procedure will be followed to correct this.

1. In this release, the RPC help and error messages for the `setexcessiveblock`
   command have been corrected to `maxBlockSize` to align the name of the
   argument, previously `blockSize`, with the name required when the
   "named argument" form of the call is used.

2. In a future release, the parameter name `maxBlockSize` introduced by
   this version will be renamed to 'excessiveBlockSize' in order to make it
   the same as the result value of the `getexcessiveblock` RPC call.


## Regressions

Bitcoin Cash Node 22.2.0 does not introduce any known regressions compared
to 22.1.0.


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

## Changes since Bitcoin Cash Node 22.1.0

### New documents

None.

### Removed documents

None.

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- ab6226ad59262fce47bfb55d9689419bb80181cf Add checkpoints for Axion activation on Bitcoin Cash
- 4ee1083307d2aaac92dd7c409cc9d6f2eb52be78 Activate Nov. 15 HF (axion) based on height on 3 of the 4 chains

#### Interfaces / RPC

- 6e398dae89614871a1e29a584a7ff52ba7a65bd6 RPC: Restore getblockheader to allow any chain, plus add test
- 4e65071f2563bb6fe004ffdc93138149aa5ef15a Update chainparams.cpp [Note: require standardness on testnet4]
- 5386f29854567c07f31ce7e92d7fc586244e0191 threads: prefix log messages with thread names
- 3de1fe89a87c1b7c1ffa4b59b9c9c4a66eba39c2 threads: add thread names to deadlock debugging message
- 7b2dfa853e484f2ee51f5279db54b07505ae5b24 util: Make thread names shorter
- a88a04310a3baa5b82065c49f942c49aa64bac5b [tx relay] Make INV broadcast interval and rate command-line options with ms resolution, default 500 ms and 7tx/MB/sec
- 2da86c683f0cb6ceb35930d8cc5c7441eaf6c430 Mining bugfix: Properly initialize `g_best_block` at startup

#### Peformance optimizations

- ff5364a5fe3415d6e14c800df810350c29ed51f4 Optimized Logger class to do less redundant copying
- 200c220fa8e7cc4806356a135ee4a20a02dc446b RPC: Significantly improve performance of `getblockstats`
- 3d917c562037c7ed2f0c85cb2b786056f99966f5 RPC: Release lock earlier in `getblock`
- cb4091ff0525ada915522558083ebccd2ce7a94c Performance nits in signature handling (#117)
- 10fe8be8d3ea9e9d550e4e526971e3811f89847a RPC: Make `getblockstats` not require txindex, use undo file instead
- 1ee2631c7c81e4af9f9f708461eb0471097e9b9c FlatFileSeq: Move directory creation out of the read only path
- a188f9be2f5f3ee41013182e9beaf89331a9eaea Upgrade UniValue code in JSON-RPC server/client
- 13d28d35464d29ef9d224abc2ce891ecda95992c Upgrade UniValue code in rpc/mining.cpp (#51)
- f08c7aebdea086177c7f67682fadc45c0dc62ac3 Upgrade UniValue code in rpc/blockchain.cpp (#51)
- eba4016f19486f8ea4cc1235af2ae9552487f9be Upgrade UniValue code in rpc/net.cpp, rpc/misc.cpp, rpc/abc.cpp (#51)
- 1b2804d9b921f7bee454784aebba558fcb33c755 Upgrade UniValue code in rpc/rawtransaction.cpp (#51)
- c582717917ab4869d5187e44bc67fa5051f7c5b0 Upgrade UniValue code in wallet/rpcwallet.cpp, wallet/rpcdump.cpp (#51)

#### GUI

- 0e2a4f201cac0c9f8d78c754cd6819691c4f8bb2 [qa] Adjust sizes of testnet4 and scalenet blockchains

#### Code quality

- 86be8a2f9bf66090724460b61fb0b9ab00fa6b68 Fix deprecated warnings in paymentserver.cpp
- 32be66f32375e92675f54f6add33cdccb5af8062 [trivial] Remove extraneous debug print in mininode.py on_version
- 14ea9c93a642e51a4e0e52f4ed07403462e2e0c6 refactor: Make `coinsel` an enum class
- a6f2b5fffecf3970083bf11a329300770c53630f [qa] Remove unused, commented out function 'GetPubKey'
- 71a6b5069a796942251b6b2d96a3f69df6bae79f [lint] align `getexcessiveblock` argument name to 'maxBlockSize'
- dc2dda04059872058d37848620d2ebf24b212654 Update ChainTxData
- 87bc68cf790b1ec9ece95ab6a37943b30522fa2a Make UniValue accept any integer type
- 31c1f3854e569708cdbe15e006601fabb5e176ac Consistently name the "excessive block size" concept + add a test

#### Documentation updates

- 244ccb5161d2309dc3d89feefc2ebe09aa0eeaad fix link to source of disclosure policy
- a4e19b1ed299ebac92a6630a96a50c406bc69bd5 [doc] Add section on LFS in benchmark docs
- 63e46a4c3edccaf9689fb914a50a377205169c51 mkdocs: consistent page names for release notes in navigation menu
- 14138108b7a868b8ddf44bf0501adccec8e52b81 [doc] Update test networks doc for Axion and v22.2.0 release changes
- 9a42efb0ba0b59a1f7c6661b45cd96fe6654d1fe mkdocs: Fix bullet list rendering
- 3966a24843116cace6e7e8ea4e42184cdfcefaa3 [doc] Update known GitLab labels
- 11a38ce211a4b741be3bd6271743c38572b14b50 [doc] Fix the ordered list in CONTRIBUTING.md
- 03a75f8530263426644d785aa84c13a728b17a2d [doc] Regenerate (update) the manpages and markdown docs for v22.2.0

#### Build / general

- a32092767394942d8cf20daf9789f519add4a668 Fix ODR violation leading to subtle build error on Fedora
- 75a2b7913cde1f94faf5b49b0b1bf034c2dd9fb0 threads: introduce util/threadnames, refactor thread naming
- 00c47844fbbad2e94b97d52063525ded519d4099 Fix portability issue with pthreads
- 10cead41bd9b2785d00ffa90de29163efc785c14 Don't rename main thread at process level
- 028a2a1cd0464817eda05f15742295f4b1b4d7d1 Allow to extend and override the sanitizers options
- 393e46f7c934bdbcdeb415b6dcc1441eecf5ca51 Block storage: Fix OOM crash when block is larger than block file size
- 8af5aceb5002ecef0afc8fbc1b9dd5e2f5659f64 Make -maxmempool default to 10x excessiveblocksize (e.g. 320 MB on mainet, 2560 MB on scalenet)

#### Build / Linux

No changes.

#### Build / Windows

No changes.

#### Build / MacOSX

- 7205d35b64f9ed3c91020facf70700b421a5dda4 Fix for compile problem in rpc/mining.cpp on some platforms

#### Tests / test framework

- 82adea613d110342830940b24abd7d144aeee7bb Fix `ninja check` failure on Arch Linux
- b7ce498a3754947a041e685cfdc476a69861a13e tests: add threadutil tests
- f22ed05ffe099524511162c33712a0d30ae727f3 test: Disable mockforward scheduler unit test for now
- ce525775f2a909ba7145a3cd4fde009b95a8648e Tests: fix for the infamous bchn-rpc-gbtl-bg-cleaner sometimes failing
- 4b6c9542e1f546fad33b8c7a8b20841db1020f3b Functional test for avg inv send interval
- a7f8f5e4442cf48061f7c8a744529167117aee17 qa: Add wallet_encryption error tests
- 10eb3e711db32c8f5057a852cf1d07ac197c0ae6 Add .markdownlint.json
- c7ee4fa8bf74cede7ae5b8bc44b065517fda1e1a [qa] fix unreliable wallet_basic tests
- 3745f7633bf392e4586d3373dd77ee7e80ac6b37 Improve functional test decodescript RPC
- c3d6d4c3e95495cc14113a7672f1bce845f5bea3 Functional tests: check for duplicate keys in JSON-RPC responses
- d930164eed00895682fa20f1d1c5d6f6135ce0d8 qa: Fix format of error message

#### Benchmarks

No changes.

#### Seeds / seeder software

- bba4243aa28c98eb7b0d5d617c16de20e277d4d7 [seeder] Modernize code
- 413e54fe3ff87a0048c9758b3e215bd1bc5305f0 remove bitcoinabc.org seeders

#### Maintainer tools

- 2a9e6d63deff84fea0b9e6b4b95959b859def701 [qa] Update make_seeds script and samples

#### Infrastructure

No changes.

#### Cleanup

- 4d6467ce8bc1f4b9124b3b2d997cd678bbe254e7 Update default assume valid and minimum chain work params after Axion activation

#### Continuous Integration (GitLab CI)

No changes.

#### Backports

- b8d9db8b927912901a566f49228bb38266fec04d Core: PR#18284: [backport] Work around negative nsecs bug in boost::wait_until
- e4223565b091a7c31022ce95c8dda27dc6a5fcc4 Core: PR#15615: [backport] Add log output during initial header sync
- d21e79aa4738d12d3197d7647c11f726900a58ff Core: PR#14726: (partial) Use RPCHelpMan for most RPCs
- 430a3113b3b964938ea7890836a2f85636abe069 Core: PR#18965: tests: implement base58_decode
- b7e0828555f7491a92e16f60476218d1f601a66a ABC: D5760: [DOC] Update the sanitizer documentation
