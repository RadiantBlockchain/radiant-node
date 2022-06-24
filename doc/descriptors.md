# Support for Output Descriptors in Radiant Node

Since Bitcoin ABC v0.20.7, there is support for Output Descriptors in the
`scantxoutset` RPC call. This is a simple language which can be used to
describe collections of output scripts.

This document describes the language. For the specifics on usage for scanning
the UTXO set, see the `scantxoutset` RPC help.

## Features

Output descriptors currently support:

- Pay-to-pubkey scripts (P2PK), through the `pk` function.
- Pay-to-pubkey-hash scripts (P2PKH), through the `pkh` function.
- Pay-to-script-hash scripts (P2SH), through the `sh` function.
- Multisig scripts, through the `multi` function.
- Any type of supported address through the `addr` function.
- Raw hex scripts through the `raw` function.
- Public keys (compressed and uncompressed) in hex notation, or BIP32 extended
  pubkeys with derivation paths.

## Examples

- `pk(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798)`
  represents a P2PK output.
- `pkh(02c6047f9441ed7d6d3045406e95c07cd85c778e4b8cef3ca7abac09b95c709ee5)`
  represents a P2PKH output.
- `combo(0279be667ef9dcbbac55a06295ce870b07029bfcdb2dce28d959f2815b16f81798)`
  represents a P2PK and P2PKH output.
- `multi(1,022f8bde4d1a07209355b4a7250a5c5128e88b84bddc619ab7cba8d569b240efe4,025cbdf0646e5db4eaa398f365f2ea7a0e3d419b7e0330e39ce92bddedcac4f9bc)`
  represents a bare *1-of-2* multisig.
- `sh(multi(2,022f01e5e15cca351daff3843fb70f3c2f0a1bdd05e5af888a67784ef3e10a2a01,03acd484e2f0c7f65309ad178a9f559abde09796974c57e714c35f110dfc27ccbe))`
 represents a P2SH *2-of-2* multisig.
- `pk(xpub661MyMwAqRbcFtXgS5sYJABqqG9YLmC4Q1Rdap9gSE8NqtwybGhePY2gZ29ESFjqJoCu1Rupje8YtGqsefD265TMg7usUDFdp6W1EGMcet8)`
 refers to a single P2PK output, using the public key part from the specified xpub.
- `pkh(xpub68Gmy5EdvgibQVfPdqkBBCHxA5htiqg55crXYuXoQRKfDBFA1WEjWgP6LHhwBZeNK1VTsfTFUHCdrfp1bgwQ9xv5ski8PX9rL2dZXvgGDnw/1'/2)`
 refers to a single P2PKH output, using child key *1'/2* of the specified xpub.
- `pkh([d34db33f/44'/0'/0']xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL/1/*)`
 describes a set of P2PKH outputs, but additionally specifies that the specified
 xpub is a child of a master with fingerprint `d34db33f`, and derived using path
 `44'/0'/0'`.

## Reference

Descriptors consist of several types of expressions. The top level expression is
always a `SCRIPT`.

`SCRIPT` expressions:

- `sh(SCRIPT)` (top level only): P2SH embed the argument.
- `pk(KEY)` (anywhere): P2PK output for the given public key.
- `pkh(KEY)` (anywhere): P2PKH output for the given public key (use `addr` if you
  only know the pubkey hash).
- `combo(KEY)` (top level only): an alias for the collection of `pk(KEY)` and `pkh(KEY)`.
- `multi(k,KEY_1,KEY_2,...,KEY_n)` (anywhere): k-of-n multisig script.
- `addr(ADDR)` (top level only): the script which ADDR expands to.
- `raw(HEX)` (top level only): the script whose hex encoding is HEX.

`KEY` expressions:

- Hex encoded public keys (66 characters starting with `02` or `03`, or 130
  characters starting with `04`).
- [WIF](https://en.bitcoin.it/wiki/Wallet_import_format) encoded private keys may
  be specified instead of the corresponding public key, with the same meaning.
- `xpub` encoded extended public key or `xprv` encoded private key (as defined in
  [BIP 32](https://github.com/bitcoin/bips/blob/master/bip-0032.mediawiki)).
    - Followed by zero or more `/NUM` unhardened and `/NUM'` hardened BIP32 derivation
      steps.
    - Optionally followed by a single `/*` or `/*'` final step to denote all (direct)
      unhardened or hardened children.
    - The usage of hardened derivation steps requires providing the private key.
    - Instead of a `'`, the suffix `h` can be used to denote hardened derivation.

- Optionally, key origin information, consisting of:
    - An open bracket `[`
    - Exactly 8 hex characters for the fingerprint of the key where the derivation
      starts (see BIP32 for details)
    - Followed by zero or more `/NUM` or `/NUM'` path elements to indicate unhardened
      or hardened derivation steps between the fingerprint and the key or xpub/xprv
      root that follows
    - A closing bracket `]`
- Followed by the actual key, which is either:
    - Hex encoded public keys (66 characters starting with `02` or `03`, or 130
      characters starting with `04`).
    - [WIF](https://en.bitcoin.it/wiki/Wallet_import_format) encoded private keys
      may be specified instead of the corresponding public key, with the same meaning.
-`xpub` encoded extended public key or `xprv` encoded private key (as defined
  in [BIP 32](https://github.com/bitcoin/bips/blob/master/bip-0032.mediawiki)).
    - Followed by zero or more `/NUM` unhardened and `/NUM'` hardened BIP32
      derivation steps.
    - Optionally followed by a single `/*` or `/*'` final step to denote all
      (direct) unhardened or hardened children.
    - The usage of hardened derivation steps requires providing the private key.
- Anywhere a `'` suffix is permitted to denote hardened derivation, the suffix
  `h` can be used instead.

`ADDR` expressions are any type of supported address:

- P2PKH addresses (base58, of the form `1...`). Note that P2PKH addresses in
  descriptors cannot be used for P2PK outputs (use the `pk` function instead).
- P2SH addresses (base58, of the form `3...`, defined in [BIP 13](https://github.com/bitcoin/bips/blob/master/bip-0013.mediawiki)).

## Explanation

### Single-key scripts

Many single-key constructions are used in practice, generally including
P2PK, and P2PKH. Many more combinations are imaginable, though they may
not be optimal: P2SH-P2PK, P2SH-P2PKH.

To describe these, we model these as functions. The functions `pk` (P2PK) and
`pkh` (P2PKH) take as input a public key in hexadecimal notation (which will
be extended later), and return the corresponding *scriptPubKey*. The functions
`sh` (P2SH) take as input a script, and return the script describing P2SH
outputs with the input as embedded script. The names of the functions do
not contain "p2" for brevity.

### Multisig

Several pieces of software use multi-signature (multisig) scripts based
on Bitcoin's OP_CHECKMULTISIG opcode. To support these, we introduce the
`multi(k,key_1,key_2,...,key_n)` function. It represents a *k-of-n*
multisig policy, where any *k* out of the *n* provided public keys must
sign.

### BIP32 derived keys and chains

Most modern wallet software and hardware uses keys that are derived using
BIP32 ("HD keys"). We support these directly by permitting strings
consisting of an extended public key (commonly referred to as an *xpub*)
plus derivation path anywhere a public key is expected. The derivation
path consists of a sequence of 0 or more integers (in the range
*0..2³¹-1*) each optionally followed by `'` or `h`, and
separated by `/` characters. The string may optionally end with the
literal `/*` or `/*'` (or `/*h`) to refer to all unhardened or hardened
child keys instead.

Whenever a public key is described using a hardened derivation step, the
script cannot be computed without access to the corresponding private
key.

### Key origin identification

In order to describe scripts whose signing keys reside on another device,
it may be necessary to identify the master key and derivation path an
xpub was derived with.

For example, when following BIP44, it would be useful to describe a
change chain directly as `xpub.../44'/0'/0'/1/*` where `xpub...`
corresponds with the master key `m`. Unfortunately, since there are
hardened derivation steps that follow the xpub, this descriptor does not
let you compute scripts without access to the corresponding private keys.
Instead, it should be written as `xpub.../1/*`, where xpub corresponds to
`m/44'/0'/0'`.

When interacting with a hardware device, it may be necessary to include
the entire path from the master down. BIP174 standardizes this by
providing the master key *fingerprint* (first 32 bit of the Hash160 of
the master pubkey), plus all derivation steps. To support constructing
these, we permit providing this key origin information inside the
descriptor language, even though it does not affect the actual
scriptPubKeys it refers to.

Every public key can be prefixed by an 8-character hexadecimal
fingerprint plus optional derivation steps (hardened and unhardened)
surrounded by brackets, identifying the master and derivation path the key or xpub
that follows was derived with.

### Including private keys

Often it is useful to communicate a description of scripts along with the
necessary private keys. For this reason, anywhere a public key or xpub is
supported, a private key in WIF format or xprv may be provided instead.
This is useful when private keys are necessary for hardened derivation
steps, or for dumping wallet descriptors including private key material.

### Compatibility with old wallets

In order to easily represent the sets of scripts currently supported by
existing Bitcoin ABC wallets, a convenience function `combo` is provided,
which takes as input a public key, and constructs the P2PK and P2PKH
scripts for that key.
