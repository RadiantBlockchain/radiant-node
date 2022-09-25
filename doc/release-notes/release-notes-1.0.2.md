# Release Notes for Radiant Node version 1.0.2

Radiant Node version 1.0.2 is now available from:

  <https://radiantblockchain.org>

## Overview

The purpose of this release is to enhance the OP codes to streamline induction proofs and introspection.

Note: The configuration file is now to be used from `radiant.conf` and not `bitcoin.conf`
 
## Usage recommendations

The release of Radiant Node 1.0.2 is recommended for use by everyone, immediately.

## Network changes

n/a

## Added functionality

Added additional induction related introspection OP Codes to make it easier to evaluate reference groups.

#### <inputIndex> OP_REFHASHDATASUMMARY_UTXO 

Pushes the hash256 output vector being spent by an input onto the stack.

`hash256(<nValue><hash256(scriptPubKey)><numRefs><hash(sortedMap(pushRefs))>)`

This allows an unlocking context to access any other input's scriptPubKey and determine what 'type' it is and all other details of that script.

#### <ref hash> OP_REFHASHVALUESUM_UTXOS 

Pushes the sum of all output values that matches the ref hash

## Deprecated functionality

n/a

## Modified functionality

n/a

## Removed functionality

n/a

## New RPC methods

n/a

## User interface changes

n/a

## Regressions

n/a

## Known Issues
 
n/a

---

## Changes since Radiant Node 1.0.1

Added 

### New documents

n/a

### Removed documents

n/a

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

n/a

#### Interfaces / RPC

n/a

#### Performance optimizations

n/a

#### GUI

n/a

#### Data directory changes

n/a

#### Code quality

n/a

#### Documentation updates

n/a

#### Build / general

n/a

#### Build / Linux

n/a

#### Build / Windows

n/a

#### Build / MacOSX

n/a

#### Tests / test framework

n/a

#### Benchmarks

n/a

#### Seeds / seeder software

n/a

#### Maintainer tools

n/a

#### Infrastructure

n/a

#### Cleanup

n/a

#### Continuous Integration (GitLab CI)

n/a

#### Backports

n/a
