Release Notes for Bitcoin Cash Node version 0.21.2
==================================================

Bitcoin Cash Node version 0.21.2 is now available from:

  <https://bitcoincashnode.org>


Overview
--------

This release of Bitcoin Cash Node contains many corrections and
improvements that we feel are useful, but it is strictly an optional
installation for those already running previous v0.21.x versions.

Bitcoin Cash Node started as a drop-in replacement for Bitcoin ABC for
the May 2020 network upgrade, to provide an alternative with minimal
changes necessary to disable the Infrastructure Funding Proposal (IFP)
soft forks.

With the network upgrade behind us, Bitcoin Cash Node starts to pursue its
own roadmap of improvement and innovation.

This release delivers performance improvements and a new mining RPC,
the `getblocktemplatelight` and `submitblocklight` calls.


GBT Light mining RPC added
--------------------------

Two new light-weight RPC calls were added:  `getblocktemplatelight` and
`submitblocklight`. These RPCs reduce the round-trip time for mining
software when retrieving new block templates.  Transaction data is never
sent between mining software and `bitcoind`.  Instead, `job_id`'s are
returned and `bitcoind` later reconstructs the full block based on this
`job_id` and the solved header + coinbase submitted by the miner, leading
to more efficient mining.

A full description and specification for this facility [accompanies this
release](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/blob/master/doc/getblocktemplatelight.md).

Three new CLI / configuration options were added to manage GBT Light:

- `-gbtcachesize=<n>` - Specify how many recent `getblocktemplatelight`
  jobs to keep cached in memory (default: 10). Jobs not in the memory
  cache will be loaded from disk.
- `-gbtstoredir=<dir>` - Specify a directory for storing
  `getblocktemplatelight` data (default: `<datadir>/gbt/`).
- `-gbtstoretime=<secs>` - Specify time in seconds to keep
  `getblocktemplatelight` data in the `-gbtstoredir` before it is
  automatically deleted (0 to disable autodeletion, default: 3600).

As usual, all of the above CLI arguments may also be specified in the
`.conf` file for the node (but without the preceding `-` character).


Account API removed
-------------------

The 'account' API was deprecated in ABC v0.20.6 and has been fully removed
in BCHN v0.21.2.

The 'label' API was introduced in ABC v0.20.6 as a replacement for accounts.

See the release notes from v0.20.6 for a full description of the changes
from the 'account' API to the 'label' API.


CashAddr in bitcoin-tx
----------------------

The bitcoin-tx tool now has full CashAddr support. CashAddr in JSON output
can be controlled with the new `-usecashaddr` option, which is turned off
by default, but relying on this default is deprecated.  The default will
change to enabled in v0.22. Specify `-usecashaddr=0` to retain the old
behavior.


Generation and publishing of Markdown documentation
---------------------------------------------------

New scripts have been introduced which convert UNIX manpage documents,
program help outputs (including RPC call help) to Markdown format.
This generated documentation is committed under the `doc/cli` and
`doc/json-rpc` folders.

Additionally, the Markdown files in the `doc` folder are now converted
to HTML via the `mkdocs` tool (see `mkdocs.yml` description file in
base folder of the source tree). The generated HTML is deployed to
https://docs.bitcoincash.org .


`-datacarrier` deprecated
-------------------------

The bitcoind/bitcoin-qt option `-datacarrier` is deprecated and will be
removed in v0.22. Instead, use the existing option `-datacarriersize` to
control relay and mining of OP_RETURN transactions, e.g. specify
`-datacarriersize=0` to reject them all.


Usage recommendations
---------------------

The update to Bitcoin Cash Node 0.21.2 is optional.

We recommend Bitcoin Cash Node 0.21.2 as a replacement for
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

BIP9 is inactive due to no available proposals to vote on and it may be
removed in a future release.


New RPC methods
---------------

- `listwalletdir` returns a list of wallets in the wallet directory which is
  configured with `-walletdir` parameter.
- `getblocktemplatelight` and `submitblocklight` are described in the
  dedicated section "GBT Light mining RPC added"


Low-level RPC changes
----------------------

The `-usehd` option has been finally removed. It was disabled in version
ABC 0.16.  From that version onwards, all new wallets created are
hierarchical deterministic wallets.
Version 0.18 made specifying `-usehd` invalid config.


Regressions
-----------

Bitcoin Cash Node 0.21.2 does not introduce any known regressions compared
to 0.21.1.


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

Changes since Bitcoin Cash Node 0.21.1
--------------------------------------

### New documents

- getblocktemplatelight.md
- publishing-documentation.md
- rapidcheck.md
- `doc/cli/` : command line program documentation (manual pages converted to Markdown)
    - bitcoin-cli.md
    - bitcoin-qt.md
    - bitcoin-seeder.md
    - bitcoin-tx.md
    - bitcoind.md
    - bitcoin-seeder.1
- `docs/json-rpc/` : RPC API documents (one help file for each call)
    - ([complete list of files](https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/tree/master/doc/json-rpc) omitted for brevity)


### Removed documents

- gitian-building-create-vm-fedora.md
- gitian-building-setup-gitian-fedora.md
- translation_strings_policy.md


### Notable commits grouped by functionality

#### Security or consensus relevant fixes

- No changes


#### Interfaces / RPC

- fdf771a7e99a3bde3e1344567555d721ba9b6ba5 New RPCs: getblocktemplatelight and submitblocklight
- 01cec0dc7a0d51d75882977f0b0a1f7ff3b10eab Accept CashAddr prefixes in bitcoin-tx input
- 918b068e7d59f797beb8a4c77e7865f333e23bcc Enable CashAddr by default in all binaries, add -usecashaddr option to bitcoin-tx
- 0669b995fc2483ea656dff0e4cc488a5e526042c Deprecate -datacarrier CLI option


#### Peformance optimizations

- c7bd2aaa184ce7ff8e45f24a73bca94896590c94 RPC performance enhancement for getmininginfo
- c0ae6e60c592bc0209b56fddf2f5ee6eaf422ac1 Minor performance fix for uint256 (base_blob) type
- 0d1148c08b6aa001e608c37598ad3f5c79d6a032 Update default assume valid and minimum chain work (chain params) after Phonon activation


#### GUI

- No changes


#### Code quality

- 2b9d92e889f1571bfa9be315a5df089b46c45aae Do not translate any CLI options
- 5f365586a7a309722398334efd73b1b229ddbff8 Make -regtest CLI option visible
- ae6d39856fda1c64451d164c53863ded68e3377d Nit help text for CLI option -help-debug
- bb46f3063c119cc75436e6ec3b294d2c149b003e Add CLI option -?? as shorthand for -help-debug and make it imply -?/-help/-h
- 26a79e816889da453de5812b66d620aeb1713876 Nits for -acceptnonstdtxn and -debuglogfile help texts
- 6965dcf3156d7e544563491d4736b88057e451c8 Rename listtransactions first argument to keep consistency
- eab6dd1687a6c6f5a1bb79653b997d8bf41bfbf4 Removed globals.cpp and globals.h
- c43727d124bb94701d01d52da52345fe5cf1512b Add CLI help text for -phononactivationtime and -replayprotectionactivationtime
- 5027e0ae1268b147c18f1167ccc728abd9cf339d Remove -help-debug since !242 removed the last debugging option
- 8414f9d4f957100c05ccb61ef765e7fc7086201c Use SetupHelpOptions (!254)
- 75aa2ad868ca904e13680ac1255fde8d3095eb33 Do not translate CLI help texts (!166)
- 669a431516bedfd2cbe79c1d474e451e28637b50 Use strprintf to substitute defaults
- 63285573e9e8f0a2c7aa9c723ce14aa21464d1ea Add boolean defaults to help text
- a7f17c6943b1e182680d0bd32452be0c7676acb0 Use constexpr for CLI argument defaults when possible
- c27c75cdc17470f14f287d5844ed7762f9dd2a4f Update ancient help text for -usecashaddr
- fb38782c20dccf9834f5af1953e0699057052bb1 Add -h, -help, -help-debug to list of hidden arguments in test/lint/check-doc.py (#88)
- 676307bbc9de2b273706ac6b6f84ad171781f08f Remove the obsoleted code pointed out by Dagur
- 0ab61b0011c321de4ac42c54a37c55fe20418f65 Show CLI argument aliases in help text, add -hh (#84)
- 50df1ddb1ce7295fa51ee34c5fa55122019802b4 Include possible range for -maxsigcachesize/-maxscriptcachesize in CLI help
- 441e39e39d4e299c399f9093f42826c170421299 GUI CLI options nits
- 805bb697830525d5b8859c5127cc84f9d634b81d Change some CLI options to advanced debugging, and some nits


#### Documentation updates

- 564389bd7a08361db9b4bc8c036eeddcdc320249 Add CLI documentation to mkdocs
- 74521fc057df4329ae2d1d325360e6e131a7457a [doc] Remove Fedora gitian building docs
- 5b8d8c23a9109e492801aafcd1f2a4392e4f5ed4 Added instructions for obtaining MacOSX10.14.sdk.tar.xz
- 5a04df0ff73e2a8f28f30dd31d136cb18c3fdeea Updated macdeploy/README.md
- 612d898ae2d2ec32e2c828d25930a7c9c027d5a7 Update Windows build notes
- b4eb448f165c1be12c2e980693feda16c8ea5c8d [doc] Initial getblocktemplatelight specification
- 33ed9f9cdb7a4df0ca29b7b4390807e6f63ea160 [doc] Add reference to BU getminingcandidate / submitminingsolution
- c56141a7a7e17d23746b38d1759af60355ae70ef [doc] Set a project rule that maintainers must leave merge request up at least 24 hrs for review
- b49fdf337c76f46d3e54ba3de1a938a72cc5f64a Markdown CLI documentation generator
- 3e64ad1facb2599eeab4fa93fd270ca0a659b395 [doc] Remove ABC reference from doc/README.md
- 458722fc46ce02f5fd5089b2144ce7e434bd3b19 Add bitcoin-seeder to Markdown CLI documentation generator
- 47ad7ead4e5da63acef2eb1f9eef7e96d99f4803 [doc] Document how documentation is published
- ddf5b302ef13a0cf109f9fb7d2f45ef8dbaebc95 Autogenerated JSON-RPC Markdown help
- 73ed2d56377c319f766bd5bf273813c69bdc0c01 Add subheadings to CLI docs for easy mkdocs navigation
- 234adae118833c4a7b1b317f6d8d36fd3a7a9bdf mkdocs: Fix code blocks rendering
- 62380dba23bcc090c889d077cac300292f2efbcd mkdocs navigation: group versions by brand


#### Build / general

- 65ef877f22188cefb273b7ed239a8a22de92303f Univalue lib: Require C++14 in autoconf
- fab4276be5e146f52c463845fbbcb336ef3763cc Gitian build broke for linux, this fixes it.
- 42dd536b07a3b58f9243077fb7f684106dc6fabb [build] Fix gitian-builder folder initialization; add warning if script differs
- 3ac4d02419c63c0cb47e6db4f8029a2b30acc07e Disable bitcoin-qt L10n in manpage generator


#### Build / Linux

- No changes


#### Build / MacOSX

- c15849cfd8176c6aa72668249529359af72999b2 Follow-up: Make sure MacOS Info.plist also declares 10.12 minimum


#### Tests / test framework

- fbaa4bc1f7d449d28207795c3fcf5251268dff12 Improve the listtransactions test for labels
- 2057a686b81f7e4ed38f8effdb68df348bba3f05 [tests] Add explicit tests for getblocktemplate error modes and caching behaviour
- 685a7e162a1e8fc7c184fb84445dca9c869b43b7 [test] Add test for RPC_CLIENT_NOT_CONNECTED error


#### Benchmarks

- No changes


#### Seeds / seeder software

- 6f5f6cf18ae1f378ccb02f08508ba56a707194dd Update the ABC node versions which MAY remain compatible with BCHN after May (if IFP does not activate)
- 31dee014ce371e648c09b789f2e3ba0088b43f86 Seeder nits


#### Maintainer tools

- No changes


#### Infrastructure

- No changes


#### Cleanup

- No changes


#### Continuous Integration (GitLab CI)

- 80be5f228168bed45e055cb54c90b64a4552189d [ci] Add build step for unix makefiles
- d2d25b32793c176a312658694d565aa765df50c3 [ci] Run functional tests for no-wallet build
- e47dff1e4e6a07b40020aa6c0f0d58e688727d18 [CI] Add 'needs' to depends builds
- 3298d179db64a3c17d55b34e2e4538d011a17edd [ci] Generate & deploy documentation

#### Backports

- ec92ca6caeba007b6960953d5502df0fc7efd057 ABC: D5590 Turn off ASM by default on target with no ASM support
- 602a7cd79ba342f09ce6ded8be5bf72981390d3f ABC: D5521 Fix typo in error message
- 4b5dab264c11d001a5881aa51c991123ceee5d2f ABC: D5542 Disable ASM for native executables
- f985d44e4397e087f4cdf754fd10b6559d307708 ABC: D5480 CMake: Build the ARM ASM field implementation
- 41f98f241721b215d186857c2dbefe7bd05e95f0 ABC: D5729 Speed up OP_REVERSEBYTES test significantly
- 1490237f977cc94910d9ec17653caf280022d3d6 ABC: D5504 Core: PR#12402, PR#12466 expat 2.2.5, miniupnpc 2.0.20180203
- 4d1ebac5fde9ce8d463b776e5d2ede3a2888423e ABC: D5494 Core: PR#15788 Rename test_bitcoin to test/setup_common
- 7829f186b811333b6e068a83082f6e259cb2c412 ABC: D5454 Improve FindZeroMQ
- 8f2cf5457100bfe451cdb194bf7f2fea3e4fed84 ABC: D5453 Improve FindSHLWAPI
- ad98e4aaa9193c9f8ccbb7fd3e90997985e0a17a ABC: D5451 Improve FindQREncode
- 5c6e3d7eb09a98e1be804565bfe4ec6becec9c7e ABC: D5450 Improve FindMiniUPnPc
- 52335eeaa406afccc85bd906f30e042da3c1bcf4 ABC: D5449 Improve FindGMP
- c077fc43cb928c33fa1f969c1535ea4ae9acc79d ABC: D5778 Core: PR#14272 init: Remove deprecated args from hidden args
- 696e5a7cce7899b72f34063b8ee42f0c9c21af8e ABC: D5452 Make the FindRapicheck module consistent with the other modules
- 07762066d8ba49fcf6e30852784913bb097247eb ABC: D5527 Core: PR#14291 wallet: Add ListWalletDir utility function
- d1bf0aaf48f2b9526be5cc337c2edea4e1580394 Core PR#15358 util: Add SetupHelpOptions()
- ddfb4f23aa9361a91efc79498907747386bf91c2 ABC: D5373 Core: PR#15948 scripted-diff: replace chainActive -> ::ChainActive()
- 8e404431cc33fd7207a022b803a9f8ecba953c8a ABC: D5056 Core: PR#15948 refactoring: introduce unused ChainActive()
- ed060277a7eb6a1eb4bb2bd1e62b689ae2ccdc0d ABC: D5445 Core: PR#16046 util: Add type safe GetTime
- 26601c53585199f926cd6b6b5bfef3742b4b83d0 ABC: D5507 Core: PR#14411 [wallet] Restore ability to list incoming transactions by label
- 72a527512d44b75ac5cfecb6d130c1ee637be463 ABC: D5517 Core: PR#14373 Consistency fixes for RPC descriptions
- 05ac2e0dbb64cd7b55ec9e7332662c0db8b21d85 ABC: D5338 [CMAKE] Fix the build with Xcode as a generator
- a6c6a799f327126abc545f5fc8ee5876e1566bb7 ABC: D5326 Core: PR#16654 build: update RapidCheck Makefile
- 7fa1429b8eeae0633f6cfe0b9de31104079ba81e ABC: D5325 Core: PR#16271 build: dont compile rapidcheck with -Wall
- 971ca32b9d57a92574814eca7a213ef3c215d105 ABC: D5324 Core: PR#14853 depends: latest rapidcheck, use INSTALL_ALL_EXTRAS
- 9613af3b3e92c01c1dab29c1808462ac5bfa734f ABC: D5323 Core: PR#12775, PR#16622, PR#16645 Integration of property based testing into Bitcoin Cash Node
- 4e61af73d7d8528305102058d466738a0b791a65 ABC: D5531 Core: PR#16392 build: macOS toolchain update
- e8997688449ea9344218cad8369246ec5d9c7702 ABC: D5422 Core: PR#13825 Remove CAccount and Update zapwallettxes comment
- 0b0b63961ecbc81a10bec72fada4374e21528ffa ABC: D5421 Core: PR#13825 Remove strFromAccount and strSentAccount
- a2c0a70a345fa29315259780efec5cfac0dbb375 ABC: D5420 Core: PR#13825 Remove fromAccount argument from CommitTransaction()
- 1074ec04a8aa90c96856d8e0b0a781eeecf1fa53 ABC: D5419 Core: PR#13825 Delete unused account functions
- 98659d9b3b0767dedb1d9276e8c726991343ca35 ABC: D5418 Core: PR#13825 Remove CAccountingEntry class
- e6f000cef02a86cdb3eaeb75e4a073b47e2aaeee ABC: D5416 Core: PR#13825 Remove ListAccountCreditDebit()
- d5b5322b7241bbe5692c7b30cd6c5aac3689f4b8 ABC: D5414 Core: PR#13825 Don't rewrite accounting entries when reordering wallet transactions and remove WriteAccountingEntry()
- 588b4da88135b4cda0767b3948ab635e8e0184e0 ABC: D5413 Core: PR#13825 Remove AddAccountingEntry()
- 842d0d6f68ce75a87ed36086fae1121d4a8c219a ABC: D5684 Use CPubKey::PUBLIC_KEY_SIZE & al when apropriate.
- aceee4be56354ef23c726cab1934e709a6fbe1dd ABC: D5308 [libsecp256k1: PR595] Allow to use external default callbacks
- 4cfeb976ba969b3691baec190e92de36c6907b40 ABC: D5446 Fix FindBerkeleyDB suffix paths
- c6d8b31fc423b788c60718c832699e31a5d7d1c7 ABC: D5339 Improve FindBerkeleyDB
- fcdc673e1d40af98c9e4f337d312d6ad4c1c7e0e ABC: D5443 check-symbols => symbol-check
- e941638c5cce8af0ee28469d3d1941fa76853167 ABC: D5387 [libsecp256k1: PR704] README: add a section for test coverage
- 350acccac75946aaade14bd38c26d268e74989b1 ABC: D5389 [libsecp256k1: PR714] doc: document the length requirements of output parameter.
- ae92f0b0f32a10b177b6ff7ad50aa112e1a261a6 ABC: D5345 Various nits in the cmake build
- d07fae9b3365ffb86656726504127c0417a674d2 ABC: D5377 [libsecp256k1: PR679] Add SECURITY.md
- 740f17b0a1c81e3cc8c092c7461ad87366bc58cc ABC: D5415 Core: PR#13825 Don't read acentry key-values from wallet on load.
- 44edc0c63bdc5f2b39c87b85d1112f4c95826f62 ABC: D5412 Core: PR#13825 Remove CWallet::ListAccountCreditDebit() and GetAccountCreditDebit()
- defd39814d4cedada3a43971b853ded54a57b5b0 ABC: D5411 Core: PR#13825 Remove AccountMove()
- a3d06f7efd100d387af0af62d056562e3bb1fc77 ABC: D5410 Core: PR#13825 Remove 'account' argument from GetLegacyBalance()
- e636e8cf788618caad24b4c528df499272393198 ABC: D5464 Core: PR#14838 Use const in COutPoint class
- 967f01e0464072cc52004e2a033ef7b97517e096 ABC: D5514 Core: PR#17550 build: set minimum supported macOS to 10.12
- 66960ba7186c217f930a4787d2512b747375e6d7 ABC: D5617 Add version number to the seeder
- 5b294a8df88104c6d3da4af170f734b3fe26d32f ABC: D5529 Update autotools for new seeder tests
- 84d32dd0effef53e7d1f12f6f7960a4012f31288 ABC: D5469 Add some unit tests for write_name() for seeder
- 75e78bef883aa4ef0bbdcf95f7b277ddbf8ef84a ABC: D5471 Add enum for parse_name() return value
- 573b60b45746dad9b1ddc9e0d62ebea6afabc75e ABC: D5459 Add constants to dns.h
- 31e5986efafa85996b0240d2b83eefd41e5cc42c ABC: D5466 Simplify max query name length check in parse_name()
- acacc57a67979b15d51bce3bdd3acf34cb015853 ABC: D5401 Rename seeder_tests to p2p_messaging_tests
- 2fd101deb5bb02bbd0b4bc6ee5d66908a7bb36aa ABC: D5091 Make parse_name() fail when passed buffer size = 0
- 041694efc74a7d3284e26ac3cc94a918c14c8501 ABC: D5329 Enforce maximum name length for parse_name() and add unit tests
- 8bfa0835d27e717ba571ea9295f15d2d77e8c153 ABC: D5328 Label length unit tests for parse_name()
- 1989fb52acdf8b0e36c912c2bdb044ad8e8d7bfb ABC: D5402 Move code in seeder_test.cpp close to where it is used
- 000c8f9fdfe96fe597eb7769826864b5df37d6a0 ABC: D5344 Remove GotVersion()
- fde461960e294b8f7587ef21da073f61dde21db1 ABC: D5489 Core: PR#14244 amount: Move CAmount CENT to unit test header
- 0052ac336d5721214422e56ec3f04cedd51884ae ABC: D5538 Core: PR#14192 utils: Convert fs error messages from multibyte to utf-8
- e0187fd646adbfe97e5f9f055014bd2f113db8ec ABC: D5536 Add verification routine to the test framework schnorr signature facility
- 04f2cfe992777ae7e09f82979a6508867c140ca4 ABC: D5533 Nits in rwcollection.h
- fba296ce9bdd28a84e7d00c4f2f1237dd29d93a5 ABC: D5518 Core: PR#14208 [build] Actually remove ENABLE_WALLET
- 6380b4c592319046deae07ed5c86d16a217376da ABC: D5520 Core: PR#12490 Some left overs from PR12490
- 9b14543a7cc8e31bb8627f09ebf1a4bbd88d032e ABC: D5526 Remove unused misc.h
- 2084301181031bec8e40c7643f2bb308d4656f4b ABC: D5523 Core: PR#14718 Remove unreferenced boost headers
- 4d8b6f15894dc4683a1e74398cd300f731aa5fd8 ABC: D4627 Core: PR#15295 qa: Add test/fuzz/test_runner.py
- 82462095f1e28a9062550ac1e2f71005f857f650 ABC: D5503 Core: PR#12607 depends: Remove ccache
- 4e76373a96d27e1472e512026aee25e3263731c4 ABC: D5502 Core: PR#10628 expat 2.2.1
- 39edaef897c27780c8f944cd9414138b4e0254a2 ABC: D5501 Improve the toolchain files
- 4e677069ad25301acf9bccc7239110950149b1f7 ABC: D5488 Core: PR#14282 [wallet] Remove -usehd
- 9d2f6846043550b4bd58440479b6237dbd63a847 ABC: D5487 Core: PR#14215 [qa] Use correct python index slices in example test
- f62ac93aeebf7c41a71092f88734ce907f47c33c ABC: D5485 Core: PR#14013 [doc] Add new regtest ports in man following #10825 ports reattributions
- 4adf56d4195d4c2848d186c5cb2534151bc39fcc ABC: D5470 Add << operator overload for PeerMessagingState
- 93570b9a242f65209c8e074209a0a34797dcb0d2 ABC: D5481 Core: PR#15504 test: Remove useless test_bitcoin_main.cpp
- b9562823f882af2646677c2e788be1508213f891 ABC: D5458 Adds PeerMessagingState enum to seeder/bitcoin.*
- a9f08fa0c5976f0e2cc8b64cea217b1c6e9ff98e ABC: D5468 Various nits in arith_uint256.h
- 5ff926e31429af0e800482fba8f38a27330ec0f7 ABC: D5463 Update dependencies in debian/control
- 95d3c792f462b4098066f901b97b9b4dba67187c ABC: D5467 Bump debian package compat level to 9
- 83f65b78437652772697e293dc2f6348efde5575 ABC: D5457 Nits to streams.h
- e723b384342342f63db3a259ad4ed231c326e56c ABC: D5447 Core PR#17266 Rename DecodeDumpTime to ParseISO8601DateTime and move to time.cpp
- 8e1180c3f7577cbd5055691e3271bc16f239cbf1 ABC: D5448 Misuse of the Visual Studio version preprocessor macro
- 49bf5993b71d438b8ee8ad2cf8d866cc8baa26f2 ABC: D5399 Move PackageOptions out of cmake/modules
- f0f6bd80bfa85e2463a1af4b8894b88cde1fb221 ABC: D5442 Don't enable the secp256k1 multiset module when building bitcoin abc
- 5ecfa80e1fd4c7c0bd77288996e0393796ed1132 ABC: D5441 check-security => security-check
- 41a71b727415647d14e93529927e91219d18bae6 ABC: D5435 Core PR#13265 wallet: Exit SyncMetaData if there are no transactions to sync
- e2d0c384e22319a74d2bd694844c8c8411d45726 ABC: D5316 Core PR#14023 Remove accounts rpcs
- 1843ea1f9a3716cc6cbd472c39bed959de2f2368 Core #13282 trivial: Mark overrides as such.
- 2b0502c17995b8cacbf87b10450da52db31dabf6 ABC: D5408 Fix Travis failures due to APT addon
- 1c2ba189309f1b8168e4fc79b07eb087217446b4 ABC: D5407 Fix travis failure on ECMULT_GEN_PRECISION
- 6cc1acb59a42b535e08e68e5b1a7929eaab75d5a Core: PR#13592 Docs: Modify policy to not translate command-line help
- 28ae2ffd310307b56e9c40605a248a353895f507 ABC: D5397 Various updates to cmake/ninja
- ea6d34d708f5195bd3ac9e9dbfe0a3093682e58f ABC: D4418 Add simple unit tests for parse_name()
- 9123db3231eb4d8d82c9533722796193173e978a ABC: D5374 [libsecp256k1: PR678] Preventing compiler optimizations in benchmarks without a memory fence
- 10ce3285869cab4014e25a0f85b0b6ed807102af ABC: D5378 [libsecp256k1: PR689] Convert bench.h to fixed-point math
- 093aeafe6056cb6e5fce05b646069e334aa54070 ABC: D5390 [libsecp256k1: PR718] Clarify that a secp256k1_ecdh_hash_function must return 0 or 1
- 5859c030b92ba6109268776ff68a78111465b112 ABC: D5362 [libsecp256k1: PR337] variable signing precompute table
- 13c28a905155716ba8b8c8fd2aa8874d5d85c481 ABC: D5388 [libsecp256k1: PR713] Docstrings
- 358200ce3aaefe8d0103aeae52c35c1129c36cc7 ABC: D5055 [Core (partial): PR#15948] rename: CChainState.chainActive -> m_chain
- f5c4241ce3640fed2f10bbdb741b83db937e67af ABC: D5371 Make CPack email available for all generators
- 15f32b97fc47bfc3866d05c2eaa384134f46464b ABC: D5376 Fix make dist by finishing RPM cleanup
- 9578be2a89e95a88a33bf21fbeece28391e2a90f ABC: D5375 Add comment on libsecp256k1 benchamrks
- 73021d20017bccb1fdb3176107e3de36e15354a2 ABC: D5365 [libsecp256k1: PR647] Increase robustness against UB in secp256k1_scalar_cadd_bit
- a86134e0c5feca063075b446140c42aae4cd5bb8 ABC: D5364 Remove mention of ec_privkey_export because it doesn't exist
- a9615da1a7e1046b3e53cc4dbd1ba0459dd4e045 ABC: D5366 Remove note about heap allocation in secp256k1_ecmult_odd_multiples_table_storage_var
- aeeb46a9bb54dead29316496f596dcd585d00929 ABC: D5319 Move lcov-filter.py to cmake/utils
- b7b0b65b020a41ecda465c7a7050aab4924ec57b ABC: D5333 Fix build with make as a generator
- 312d89416054e1516d16b3aff31a21ee69e13818 ABC: D5363 Fix the benchmark build when wallet is disabled
- 56bb0b7a52a1db2eaae2b384fea00bd76cec587d ABC: D5353 [libsecp256k1: PR583] JNI: fix use sig array
- 13bcd5753fc1369c553aed58bcb5e6b8fc60a36c ABC: D5340 Make the test python scripts depend on targets and not on files
- f91260b62db0ef9819dc04aee79e1b4b2a41fbd4 ABC: D5347 [libsecp256k1: PR629] Avoid calling secp256k1_*_is_zero when secp256k1_*_set_b32 fails.
- 2f5fe9f6f17ce85c5dd6da5273d2240cacc35be9 ABC: D5349 [libsecp256k1: PR634] Add a descriptive comment for secp256k1_ecmult_const.
- 2b7c674fcb43f71e19ff4be52e6bd112e1007c15 ABC: D5356 [libsecp256k1: PR650] secp256k1/src/tests.c: Properly handle sscanf return value
- 201d3259f5de1a17d61d22c20a5942c6d4cdb159 ABC: D5351 [libsecp256k1: PR651] Fix typo in secp256k1_preallocated.h
- 4822db0807d5a5e95de9c93b97d366f9f17f2aa5 ABC: D5357 [libsecp256k1: PR657] Fix a nit in the recovery tests
- f5b2b1f742a6e845e3fba8a3d3919f8ff90fc912 ABC: D5350 [libsecp256k1: PR640] scalar_impl.h: fix includes
- a3d6deb5c6b3045e8d699febd3e09694140b517a ABC: D5352 [libsecp256k1: PR644] Moved a dereference so the null check will be before the dereferencing
- 569848a3af33053865b4626f9f30976428ab5c30 ABC: D5295 Core: PR#14307 Consolidate redundant implementations of ParseHashStr
- 020cbabcca089d838315b68059c3fb233fcf2b24 ABC: D5312 Make the list of tests a property of the test suite
- 96fae32cc61b5a6e8a75d76629536dd0c63d5336 ABC: D5303 Core: PR#17647 lcov: filter depends from coverage report
- 25acd3ce4e798f3bcb39f079a887fbbb776c6338 ABC: D5302 Core: PR#16207 Failing functional tests stop lcov
- ca25e2d1636d10971dba4a415009c75d6f6462d4 ABC: D6090 Fix incorrect mocktime set in [no] miner fund test
- 29bd0ad79debba10341da6a1508223eb6b9f90e4 ABC: D6094 Add checkpoints for phonon activation

