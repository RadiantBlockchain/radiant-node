Test Networks
=============

This document describes the Radiant test networks supported by the
RADN software.

There are currently three test networks that you can access with RADN:

- testnet3 (historical testnet)
- testnet4
- scalenet

These test network are maintained and supported by the wider community
of protocol developers. They can be accessed by running the software
(daemon, GUI and CLI) with `-testnet`, `-testnet4` and `-scalenet`
arguments, respectively.

Other software clients may have additional test network definitions compiled
into them but these are not currently supported by RADN and could not be
accessed without further modifications to the software. If you wish RADN
to access a test network not listed above, please raise a support request.

Below, we give a brief description and an overview table for these networks.

Testnet3
--------

This is the historical testnet in Radiant, maintained as a fork from
BTC's testnet3 since 2017. It has grown substantially in size
(2020/Oct/15: 44GB), in part due to scaling tests that deposited a number
of 32MB blocks, and due to the resulting time to sync a test node from
scratch, has become inconvenient for quick tests.

Testnet4
--------

Testnet4 is a testnet3 replacement (starting from a fresh genesis block)
intended to be kept light-weight and quick to sync, in other words free of
big block 'spam'.

Testnet4 has a reduced default blocksize to discourage high throughput and
difficulty algorithm settings adjusted to make sure it recovers to be
CPU-mineable quickly after someone has used an ASIC on it.

Scaling tests should use 'scalenet' instead (see next section).

Scalenet
--------

Scalenet is intended as a place to test application performance in
high-throughput situations (bigger blocks, more transactions etc).

Scalenet will have a default blocksize limit a few times higher than
mainnet's limit, to serve as a proving ground for future scaling.

ASIC mining on scalenet will be encouraged and the mining difficulty will
adjust slower to allow accurate exploration of mining strategies.

Every 6 months or so, scalenet's block 10,000 will be invalidated and a new
block will be checkpointed in its place, clearing out the previous high volume
history and keeping scalenet semi-affordable to synchronize.

Scalenet is intended to target the performance level of a ~$40/month VPS
or a $500 desktop computer for the near future. Any tests that target higher
performance levels are encouraged to do so by forking off of scalenet or
creating their own private testnets or regtest networks.

Overview Table for RADN-supported Test Networks
-----------------------------------------------

| Attribute/Network            |  testnet3   |   testnet4   |  scalenet   |
|------------------------------|-------------|--------------|-------------|
| Default p2p port             |  18333      |  28333       |  38333      |
| Network magic bytes          |  0xf4e5f3f4 |  0xe2b7daaf  |  0xc3afe1a2 |
| CashAddr prefix              |  bchtest    |  bchtest     |  bchtest    |
| Default excessive block size |  32MB       |  2MB         |  256MB      |
| Block Target spacing         |  10 min     |  10 min      |  10 min     |
| POW limit                    |  2^224      |  2^224       |  2^224      |
| ASERT half-life              |  1 hour     |  1 hour      |  2 days     |
| Allow min diff blocks        |  yes        |  yes         |  yes        |
| Require standard txs         |  no         |  yes         |  no         |
| Default consist. chks.       |  no         |  no          |  no         |
| Halving interval (blks)      |  210000     |  210000      |  210000     |
| BIP16 height                 |  514        |  1           |  1          |
| BIP34 height                 |  21111      |  2           |  2          |
| BIP65 height                 |  581885     |  3           |  3          |
| BIP66 height                 |  330776     |  4           |  4          |
| CSV height                   |  770112     |  5           |  5          |
| UAHF (BCH fork) height       |  1155875    |  6           |  6          |
| Nov 13 2017 HF height        |  1188697    |  3000        |  3000       |
| Nov 15 2018 HF height        |  1267996    |  4000        |  4000       |
| Nov 15 2019 HF height        |  1341711    |  5000        |  5000       |
| May 15 2020 HF height        |  1378460    |  0 (Note 1)  |  0 (Note 1) |
| Nov 15 2020 HF height        |  1421482    |  16845       |  variable (Note 2) |
| Base58 prefix: pubkey        |  1, 111     |  1, 111      |  1, 111     |
| Base58 prefix: script        |  1, 196     |  1, 196      |  1, 196     |
| Base58 prefix: seckey        |  1, 239     |  1, 239      |  1, 239     |
| Base58 p: ext. pubkey        |  0x043587cf |  0x043587cf  |  0x043587cf |
| Base58 p: ext. seckey        |  0x04358394 |  0x04358394  |  0x04358394 |

Note 1: set to 0 because historical sigop code has been removed from RADN
        See chainparams.cpp for more detailed comments.

Note 2: scalenet is intended to be periodically reorganized down to a
        height of 10000 whose earlier than the November 2020 MTP activation
        time. The height at which the Axion upgrade takes effect is thus
        variable (it is block 16869 now, but may be different once the
        network is reset).

Further references
------------------

1. <https://bitcoincashresearch.org/t/testnet4-and-scalenet/148>
