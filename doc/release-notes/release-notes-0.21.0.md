Release Notes for Bitcoin Cash Node version 0.21.0
==================================================

Bitcoin Cash Node version 0.21.0 is now available from:

  <https://bitcoincashnode.org>

This is the first release of Bitcoin Cash Node as a drop-in replacement for Bitcoin ABC 0.21.0. It is based on Bitcoin ABC 0.21.0, with minimal changes necessary to disable the Infrastructure Funding Proposal (IFP) soft forks. For exchanges and users, this client will follow the longest chain whether it includes IFP soft forks or not. For miners, running this client ensures the `getblocktemplate` RPC call will return a block with version bits that vote "NO" for the IFP soft forks. Additionally, unlike Bitcoin ABC, `getblocktemplate` will *not* automatically insert IFP white-list addresses into the coinbase transaction.

Minimal changes from Bitcoin ABC 0.21.0 to Bitcoin Cash Node 0.21.0:
- All IFP soft fork logic, signaling logic and the hard-coded whitelist have been removed.
- Rebranding from Bitcoin ABC to Bitcoin Cash Node.
- Qt GUI settings are automatically copied from Bitcoin ABC on first use of Bitcoin Cash Node.

_Note regarding BIP9 and `getblockchaininfo` below: BIP9 is inactive due to no available proposals to vote on and it may be removed in a future release._

All other upgrade changes from ABC 0.21.0 are untouched and included below for reference.

----

This release includes the following features and fixes:

- The RPC `getrpcinfo` returns runtime details of the RPC server.
At the moment it returns the active commands and the corresponding execution time.
- `ischange` field of boolean type that shows if an address was used for change output was added to `getaddressinfo` method response.
- Bump automatic replay protection to Nov 2020 upgrade.
- Re-introduction of BIP9, info available from the `getblockchaininfo` RPC.
- Various bug fixes and stability improvements.

New RPC methods
---------------

- `getnodeaddresses` returns peer addresses known to this node.
  It may be used to connect to nodes over TCP without using the DNS seeds.

Network upgrade
---------------
At the MTP time of 1589544000 (May 15, 2020 12:00:00 UTC) the following behaviors will change:

- The default for max number of in-pool ancestors (`-limitancestorcount`) is changed from 25 to 50.
- The default for max number of in-pool descendants (`-limitdescendantcount`) is changed from 25 to 50.
- OP_REVERSEBYTES support in script.
- New SigOps counting method (SigChecks) as standardness and consensus rules.

Usage recommendations
---------------------

We recommend Bitcoin Cash Node 0.21.0 as a drop-in replacement for ABC 0.21.0.

The files in this release contain some resource links such as email addresses
(e.g. info@bitcoincashnode.org) which are still being brought into operation and
may be unavailable for some time after the initial release. We ask for your
patience and will advise when ready via the project's communication channels
(website, Slack, Telegram, Twitter, Reddit).

Regressions
-----------

Bitcoin Cash Node 0.21.0 does not introduce any known regressions compared to ABC 0.21.0.
