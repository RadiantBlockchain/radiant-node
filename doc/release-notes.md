Release Notes for Bitcoin Cash Node version 0.21.2
==================================================

Bitcoin Cash Node version 0.21.2 is now available from:

  <https://bitcoincashnode.org>


Overview
--------

...


Account API removed
------------------
 - The 'account' API was deprecated in ABC v0.20.6 and has been fully removed in v0.21
 - The 'label' API was introduced in ABC v0.20.6 as a replacement for accounts.

 - See the release notes from v0.20.6 for a full description of the changes from the
'account' API to the 'label' API.


CashAddr in bitcoin-tx
----------------------
The bitcoin-tx tool now has full CashAddr support. CashAddr in JSON output can be controlled with the new `-usecashaddr` option, which is turned off by default, but relying on this default is deprecated. The default will change to enabled in v0.22. Specify `-usecashaddr=0` to retain the old behavior.


Usage recommendations
---------------------

...

MacOS versions earlier than 10.12 are no longer supported.
Additionally, Bitcoin Cash Node does not yet change appearance when macOS
"dark mode" is activated.


Note regarding BIP9 and `getblockchaininfo`
-------------------------------------------

BIP9 is inactive due to no available proposals to vote on and it may be
removed in a future release.


New RPC methods
------------
 - `listwalletdir` returns a list of wallets in the wallet directory which is
   configured with `-walletdir` parameter.


Low-level RPC changes
----------------------
The `-usehd` option has been finally removed. It was disabled in version ABC 0.16.
From that version onwards, all new wallets created are hierarchical
deterministic wallets. Version 0.18 made specifying `-usehd` invalid config.


Regressions
-----------

...


Known Issues
------------

...


---

Changes since Bitcoin Cash Node 0.21.1
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

Build / general:
...

Build / Linux:
...

Build / MacOSX:
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
