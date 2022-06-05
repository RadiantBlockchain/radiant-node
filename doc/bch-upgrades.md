Currently supported Radiant upgrades
=========================================

This page lists the Radiant upgrade proposals that have currently been
implemented in Radiant Node, along with their implementation status. Note
that the Radiant Node project co-publishes the specifications of all Bitcoin
Cash upgrades through a [separate repository](https://upgradespecs.radiantblockchain.org).

For implementation status of Bitcoin (Core) proposals in Radiant Node,
see [BIPs](bips.md).

Network upgrades
----------------

* **[August 1st 2017](https://upgradespecs.radiantblockchain.org/uahf-technical-spec/)** (*UAHF*) upgrade implementation completed in [v0.14.5](release-notes/release-notes-0.14.5.md):
    * REQ-1: The fork is enabled by default as of [v0.14.1](release-notes/release-notes-0.14.1.md) ([D221](https://gitlab.com/radiant-node/radiant-node/-/commit/c2399c92935ef13f0d9e2f972eeb8455a8e787a2)).
    * REQ-2: Activation time can be configured with the `-uahfstarttime` option ([D221](https://gitlab.com/radiant-node/radiant-node/-/commit/c2399c92935ef13f0d9e2f972eeb8455a8e787a2)) and the `setuahfstarttime` RPC command ([D243](https://gitlab.com/radiant-node/radiant-node/-/commit/cbdaf5b22f6183f4d07f79d5079cc1de72ce4daf)) as of [v0.14.1](release-notes/release-notes-0.14.1.md).
    * REQ-3: Fork block must be >1MB as of [v0.14.1](release-notes/release-notes-0.14.1.md) ([D200](https://gitlab.com/radiant-node/radiant-node/-/commit/6c6719b09f06ae4ac631d0e74d31001cdfc9fd1f)).
    * REQ-4-1: Block size limit increases to 8MB by default ([D207](https://gitlab.com/radiant-node/radiant-node/-/commit/00cb8ae2a2389d9e2b1f6861b238ca025911fe97)) and this must be set >1MB ([D240](https://gitlab.com/radiant-node/radiant-node/-/commit/0636ab3cc0602e45ed0316aebccddc7ca69f6bcf)), as of [v0.14.1](release-notes/release-notes-0.14.1.md).
    * REQ-4-2: Generated block size limit increases to 2MB by default ([D207](https://gitlab.com/radiant-node/radiant-node/-/commit/00cb8ae2a2389d9e2b1f6861b238ca025911fe97)) and this must be set >1MB ([D240](https://gitlab.com/radiant-node/radiant-node/-/commit/0636ab3cc0602e45ed0316aebccddc7ca69f6bcf)), as of [v0.14.1](release-notes/release-notes-0.14.1.md).
    * REQ-5: Block SigOps limit relaxes ([D127](https://gitlab.com/radiant-node/radiant-node/-/commit/5a42f155ffc30b89065befca458ffdb842524544)) while transaction size limit ([D8](https://gitlab.com/radiant-node/radiant-node/-/commit/48dc7934dc0b09260d89662f7604f9d5309ae52e)) and transaction SigOps limit ([D129](https://gitlab.com/radiant-node/radiant-node/-/commit/1af54d43495463c22d906da98a28317895e545ed)) remain, as of [v0.14.1](release-notes/release-notes-0.14.1.md).
    * REQ-6-1: Magic OP_RETURN value temporarily becomes disallowed as of [v0.14.1](release-notes/release-notes-0.14.1.md) ([D148](https://gitlab.com/radiant-node/radiant-node/-/commit/cddb1bbbe6c934a57ce49f286d72cb027830cd8a), [D281](https://gitlab.com/radiant-node/radiant-node/-/commit/69ef458403a5f9cf7106ed288e282fcf6d08c89b)).
    * REQ-6-2: SIGHASH_FORKID becomes mandatory as of [v0.14.5](release-notes/release-notes-0.14.5.md) ([D371](https://gitlab.com/radiant-node/radiant-node/-/commit/e49826c1fcc36e5ae26de0ad4d06e2063a759e73)).
    * REQ-6-3: [Replay-protected sighash format](https://upgradespecs.radiantblockchain.org/replay-protected-sighash/) becomes allowed as of [v0.14.1](release-notes/release-notes-0.14.1.md) ([D68](https://gitlab.com/radiant-node/radiant-node/-/commit/db6218a119dda2ed09d42bb45e44abff9810d7ec#4991ff4d3409dea6845eb786eea9b14f5b78b1cd)).
    * REQ-6-4: STRICTENC enforcement begins as of [v0.14.5](release-notes/release-notes-0.14.5.md) ([D371](https://gitlab.com/radiant-node/radiant-node/-/commit/e49826c1fcc36e5ae26de0ad4d06e2063a759e73)).
    * REQ-7: Effective immediately, Emergency Difficulty Adjustment is enabled as of [v0.14.2](release-notes/release-notes-0.14.2.md) ([D298](https://gitlab.com/radiant-node/radiant-node/-/commit/7ad1105f43d7bff158d4b5c882ab9bf1b74d6cce)).
    * REQ-DISABLE: The possibility to disable the fork by setting activation time to zero is *[not implemented](https://reviews.bitcoinabc.org/T54)*.
    * OPT-SERVICEBIT: The `NODE_BITCOIN_CASH` service bit is set as of [v0.14.5](release-notes/release-notes-0.14.5.md) ([D366](https://gitlab.com/radiant-node/radiant-node/-/commit/bfd7b2222ef07b96dd4868d2c04130193da3468e)).
* **[November 13th 2017](https://upgradespecs.radiantblockchain.org/nov-13-hardfork-spec/)** (*Cash Hard Fork*) upgrade implementation completed in [v0.16.0](release-notes/release-notes-0.16.0.md):
    * LOW_S and NULLFAIL enforcement begins as of [v0.16.0](release-notes/release-notes-0.16.0.md) ([D616](https://gitlab.com/radiant-node/radiant-node/-/commit/aeb72d7f3c737947090884390fbe28d00e4e0621)).
    * New Difficulty Adjustment Algorithm takes effect as of [v0.16.0](release-notes/release-notes-0.16.0.md) ([D601](https://github.com/Bitcoin-ABC/bitcoin-abc/commit/be51cf295c239ff6395a0aa67a3e13906aca9cb2), [D628](https://github.com/Bitcoin-ABC/bitcoin-abc/commit/18dc8bb907091d69f4887560ab2e4cfbc19bae77)).
* **[May 15th 2018](https://upgradespecs.radiantblockchain.org/may-2018-hardfork/)** (*Monolith*) upgrade implementation completed in [v0.17.0](release-notes/release-notes-0.17.0.md):
    * Default block size limit increases as of [v0.17.0](release-notes/release-notes-0.17.0.md) ([D1149](https://gitlab.com/radiant-node/radiant-node/-/commit/699f4b867318486b915bd2d3b2102fb49ec652f1)).
    * [Opcodes are re-enabled](https://upgradespecs.radiantblockchain.org/may-2018-reenabled-opcodes/) as of [v0.17.0](release-notes/release-notes-0.17.0.md) ([D1231](https://gitlab.com/radiant-node/radiant-node/-/commit/f103591b993fef4359819bd1fb956f47e7b540e2)).
    * Automatic Replay Protection has been scheduled for November 15th 2018 in [v0.17.0](release-notes/release-notes-0.17.0.md) ([D1199](https://gitlab.com/radiant-node/radiant-node/-/commit/db0e07afa96e965c9ec3e70b794009c02be48198)).
    * Default OP_RETURN limit increases as of [v0.17.0](release-notes/release-notes-0.17.0.md) ([D1158](https://gitlab.com/radiant-node/radiant-node/-/commit/cbf4410912f6512e481f15270329683d4d4378d4), [D1205](https://gitlab.com/radiant-node/radiant-node/-/commit/0d1b49c9f37f8549540521e6e02a27c261c6da5a)).
* **[November 15th 2018](https://upgradespecs.radiantblockchain.org/2018-nov-upgrade/)** (*Magnetic Anomaly*) upgrade implementation completed in [v0.18.0](release-notes/release-notes-0.18.0.md):
    * Topological transaction order is replaced with canonical transaction order as of [v0.18.0](release-notes/release-notes-0.18.0.md) ([D1529](https://gitlab.com/radiant-node/radiant-node/-/commit/ee51761f7792776ddde50aaa0c700aea2529fa3c)).
    * [OP_CHECKDATASIG and OP_CHECKDATASIGVERIFY](https://upgradespecs.radiantblockchain.org/op_checkdatasig/) are added as of [v0.18.0](release-notes/release-notes-0.18.0.md) ([D1625](https://gitlab.com/radiant-node/radiant-node/-/commit/13eb8667a8073ee39f61039bbf3c7a172784a523), [D1646](https://gitlab.com/radiant-node/radiant-node/-/commit/bcaa59bb2fbeec1811696a99a1dddf9530126b1c), [D1653](https://gitlab.com/radiant-node/radiant-node/-/commit/497a1b485ba930c39ce9132d7202137cfec8298f)).
    * Minimum transaction size is enforced as of [v0.18.0](release-notes/release-notes-0.18.0.md) ([D1611](https://gitlab.com/radiant-node/radiant-node/-/commit/de3668a2b57239c2a223900d7e96158a6af72ab4)).
    * SIGPUSHONLY enforcement begins as of [v0.18.0](release-notes/release-notes-0.18.0.md) ([D1623](https://gitlab.com/radiant-node/radiant-node/-/commit/4714cd3622565b35d08fa71d932482ad760cc0ba)).
    * CLEANSTACK enforcement begins as of [v0.18.0](release-notes/release-notes-0.18.0.md) ([D1647](https://gitlab.com/radiant-node/radiant-node/-/commit/073d453b4ae71b0744e4b1b723066373a3b80acb)).
    * Automatic Replay Protection has been rescheduled to May 15th 2019 in [v0.18.0](release-notes/release-notes-0.18.0.md) ([D1612](https://gitlab.com/radiant-node/radiant-node/-/commit/92da404962ccc0ddaf067b94523fcdf315f44233)).
* **[May 15th 2019](https://upgradespecs.radiantblockchain.org/2019-05-15-upgrade/)** (*Great Wall*) upgrade implementation completed in [v0.19.0](release-notes/release-notes-0.19.0.md):
    * [Schnorr signatures](https://upgradespecs.radiantblockchain.org/2019-05-15-schnorr/) are enabled as of [v0.19.0](release-notes/release-notes-0.19.0.md) ([D2483](https://gitlab.com/radiant-node/radiant-node/-/commit/6bb69585f3265e99d01d4fdd5fe7d48b2ee4e557)).
    * [SegWit recovery](https://upgradespecs.radiantblockchain.org/2019-05-15-segwit-recovery/) becomes allowed as of [v0.19.0](release-notes/release-notes-0.19.0.md) ([D2479](https://gitlab.com/radiant-node/radiant-node/-/commit/f19955048697770a9743458f823a6c84d8140ac4)).
    * Automatic Replay Protection has been rescheduled to November 15th 2019 in [v0.19.0](release-notes/release-notes-0.19.0.md) ([D2376](https://gitlab.com/radiant-node/radiant-node/-/commit/31427f585a5c2a2de5dcde2c041928fcdc5e7e0a)).
* **[November 15th 2019](https://upgradespecs.radiantblockchain.org/2019-11-15-upgrade/)** (*Graviton*) upgrade implementation completed in [v0.20.0](release-notes/release-notes-0.20.0.md):
    * [Schnorr multisig](https://upgradespecs.radiantblockchain.org/2019-11-15-schnorrmultisig/) is enabled as of [v0.19.12](release-notes/release-notes-0.19.12.md) ([D3736](https://gitlab.com/radiant-node/radiant-node/-/commit/2a1e1d244b1b31ac5b4a800bf085578b85a6af9f)).
    * [MINIMALDATA enforcement](https://upgradespecs.radiantblockchain.org/2019-11-15-minimaldata/) begins as of [v0.19.12](release-notes/release-notes-0.19.12.md) ([D3763](https://gitlab.com/radiant-node/radiant-node/-/commit/38d64b15884bcc0cd2e84ecc7c0fd9b3d2a50930)).
    * Automatic Replay Protection has been rescheduled to May 15th 2020 in [v0.20.0](release-notes/release-notes-0.20.0.md) ([D3868](https://gitlab.com/radiant-node/radiant-node/-/commit/65a6198254ac142dd87d3b8b6edafc49c9ef0a9c)).
* **[May 15th 2020](https://upgradespecs.radiantblockchain.org/2020-05-15-upgrade/)** (*Phonon*) upgrade implementation completed in [v0.21.0](release-notes/release-notes-0.21.0.md):
    * The SigOps system is replaced with the new [SigChecks system](https://upgradespecs.radiantblockchain.org/2020-05-15-sigchecks/) as of [v0.21.0](release-notes/release-notes-0.21.0.md) ([D5029](https://gitlab.com/radiant-node/radiant-node/-/commit/0cfa675d41f9fdb461bb8d67ca5f0fe524a57c3a), [D5179](https://gitlab.com/radiant-node/radiant-node/-/commit/276a95b8710e9202c8cc9346987f2df2aa83d72f)).
    * [OP_REVERSEBYTES](https://upgradespecs.radiantblockchain.org/2020-05-15-op_reversebytes/) is added as of [v0.21.0](release-notes/release-notes-0.21.0.md) ([D5283](https://gitlab.com/radiant-node/radiant-node/-/commit/9bd868e48eb0cc63063fd1776d2e84277a510a6b)).
    * Default in-mempool ancestors/descendants limits increase as of [v0.21.0](release-notes/release-notes-0.21.0.md) ([D5244](https://gitlab.com/radiant-node/radiant-node/-/commit/3a535f346e0b66cefddc47e8f8b9328b50e91f94)).
    * Automatic Replay Protection has been rescheduled to November 15th 2020 in [v0.21.0](release-notes/release-notes-0.21.0.md) ([D5253](https://gitlab.com/radiant-node/radiant-node/-/commit/c4fd03771c42f9955ae938c0325687215b1aac4d)).
* **[November 15th 2020](https://upgradespecs.radiantblockchain.org/2020-11-15-upgrade/)** (*Axion*) upgrade implementation completed in [v22.0.0](release-notes/release-notes-22.0.0.md):
    * [ASERT Difficulty Adjustment Algorithm](https://upgradespecs.radiantblockchain.org/2020-11-15-asert/) takes effect as of [v22.0.0](release-notes/release-notes-22.0.0.md) ([MR!692](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/692)).
    * Automatic Replay Protection has been unscheduled in [v22.0.0](release-notes/release-notes-22.0.0.md) ([MR!709](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/709), [MR!715](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/715)).
* **[May 15th 2021](https://upgradespecs.radiantblockchain.org/2021-05-15-upgrade/)** (*Tachyon*) upgrade implementation completed in v23.0.0:
    * [Unconfirmed transaction chain limit](https://upgradespecs.radiantblockchain.org/unconfirmed-transaction-chain-limit/) is removed in v23.0.0 ([MR!1130](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/1130)).
    * [Multiple OP_RETURN outputs](https://upgradespecs.radiantblockchain.org/CHIP-2021-03-12_Multiple_OP_RETURN_for_Bitcoin_Cash/) are allowed in v23.0.0 ([MR!1115](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/1115)).
* **[May 15th 2022](https://upgradespecs.radiantblockchain.org/2022-05-15-upgrade/)** (*Upgrade 8*) upgrade implementation completed in v24.0.0:
    * [CHIP-2021-03: Bigger Script Integers](https://gitlab.com/GeneralProtocols/research/chips/-/blob/master/CHIP-2021-02-Bigger-Script-Integers.md) v1.0 is implemented in v24.0.0 ([MR!1253](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/1253)).
    * [CHIP-2021-02: Native Introspection Opcodes](https://gitlab.com/GeneralProtocols/research/chips/-/blob/master/CHIP-2021-02-Add-Native-Introspection-Opcodes.md) v1.1.3 is implemented in v24.0.0 ([MR!1208](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/1208)).
    * [BIP-0069: Lexicographical Indexing of Transaction Inputs and Outputs](https://github.com/bitcoin/bips/blob/master/bip-0069.mediawiki) is implemented in v24.0.0 ([MR!1260](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/1260)).

Other
-----

* **[CashAddr](https://upgradespecs.radiantblockchain.org/cashaddr/)**: The address format for Bitcoin Cash is implemented as of [v0.16.2](release-notes/release-notes-0.16.2.md), with `bitcoin-tx` support completed in [v0.21.2](release-notes/release-notes-0.21.2.md) ([MR!274](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/274), [MR!275](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/275)). The preferred address format can be selected with the `-usecashaddr` option.
* **[Double Spend Proofs](https://upgradespecs.radiantblockchain.org/dsproof/)**: The DSProof feature is implemented as of v23.0.0 ([MR!965](https://gitlab.com/radiant-node/radiant-node/-/merge_requests/965)). See [notes on the RADN implementation](dsproof-implementation-notes.md).
* **[JSON-RPC](https://upgradespecs.radiantblockchain.org/JSON-RPC/)**: Radiant Node is compatible with the JSON-RPC specification, but the JSON-RPC functionality has significantly improved since. See our [JSON-RPC API documentation](json-rpc/README.md) for an up-to-date overview of what is available in Radiant Node.
* **[Block format](https://upgradespecs.radiantblockchain.org/block/)**: Radiant Node is compatible with the block data structure specification, albeit the transaction order has changed since the [November 15th 2018 network upgrade](https://upgradespecs.radiantblockchain.org/2018-nov-upgrade/).
* **[Transaction format](https://upgradespecs.radiantblockchain.org/transaction/)**: Radiant Node is compatible with the transaction data structure specification.

