Bitcoin ABC 0.20.0
==================

Bitcoin ABC version 0.20.0 is now available from:

  <https://download.bitcoinabc.org/0.20.0/>

This release includes the following features and fixes:

- Support for Nov 2019 upgrade features, as detailed at [https://github.com/bitcoincashorg/bitcoincash.org/blob/master/spec/2019-11-15-upgrade.md](https://upgradespecs.bitcoincashnode.org/2019-11-15-upgrade/)
    - Schnorr signatures for OP_CHECKMULTISIG(VERIFY): [https://github.com/bitcoincashorg/bitcoincash.org/blob/master/spec/2019-11-15-schnorrmultisig.md](https://upgradespecs.bitcoincashnode.org/2019-11-15-schnorrmultisig/)
    - Enforce MINIMALDATA in script: [https://github.com/bitcoincashorg/bitcoincash.org/blob/master/spec/2019-11-15-minimaldata.md](https://upgradespecs.bitcoincashnode.org/2019-11-15-minimaldata/)
- Bump automatic replay protection to May 2020 upgrade.
- Searching by transaction ID is now available in bitcoin-qt.
- Minor bug fixes.
