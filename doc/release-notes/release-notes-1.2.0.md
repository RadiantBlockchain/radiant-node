# Release Notes for Radiant Node Version 1.2.0 (Zeus)

Release Name: Zeus 

Radiant Node version 1.2.0 (Zeus) is now available from: <https://radiantblockchain.org>

## Overview

Note: Consensus updates at block height 62,000 which is estimated to be November 24, 2022 or a few days before.

Updated nomenclature to use "PHOTONS" instead of "SATOSHIS". 1 Radiant = 100,000,000 photons (PHO)

See detailed description of the OP codes below and their capabilities.

List of new OP Codes:

- `<refHash 32 bytes> OP_REFHASHVALUESUM_UTXOS` (Renamed from OP_UTXOREFVALUESUM)
- `<refHash 32 bytes> OP_REFHASHVALUESUM_OUTPUTS`
- `<inputIndex> OP_REFHASHDATASUMMARY_UTXO` (Renamed from OP_UTXODATASUMMARY)
- `<outputIndex> OP_REFHASHDATASUMMARY_OUTPUT`
- `OP_PUSHINPUTREFSINGLETON`
- `<refAssetId 36 bytes> OP_REFTYPE_UTXO`
- `<refAssetId 36 bytes> OP_REFTYPE_OUTPUT`
- `<inputIndex> OP_STATESEPARATORINDEX_UTXO`
- `<outputIndex> OP_STATESEPARATORINDEX_OUTPUT`
- `<refAssetId 36 bytes> OP_REFVALUESUM_UTXOS`
- `<refAssetId 36 bytes> OP_REFVALUESUM_OUTPUTS`
- `<refAssetId 36 bytes> OP_REFOUTPUTCOUNT_UTXOS`
- `<refAssetId 36 bytes> OP_REFOUTPUTCOUNT_OUTPUTS`
- `<refAssetId 36 bytes> OP_REFOUTPUTCOUNTZEROVALUED_UTXOS` 
- `<refAssetId 36 bytes> OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS`
- `<inputIndex> OP_REFDATASUMMARY_UTXO`
- `<outputIndex> OP_REFDATASUMMARY_OUTPUT`
- `<codeScriptHash 32 bytes> OP_CODESCRIPTHASHVALUESUM_UTXOS`
- `<codeScriptHash 32 bytes> OP_CODESCRIPTHASHVALUESUM_OUTPUTS`
- `<codeScriptHash 32 bytes> OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS`
- `<codeScriptHash 32 bytes> OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS`
- `<codeScriptHash 32 bytes> OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS`
- `<codeScriptHash 32 bytes> OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS`
- `<inputIndex> OP_CODESCRIPTBYTECODE_UTXO`
- `<outputIndex> OP_CODESCRIPTBYTECODE_OUTPUT` 
- `<inputIndex> OP_STATESCRIPTBYTECODE_UTXO`
- `<outputIndex> OP_STATESCRIPTBYTECODE_OUTPUT`

Additional improvements:

- Removed validating induction OP codes on rolling a block forward as the blocks were already validated, and the extra overhead is not needed
- Correctly print "Radiant server starting" on startup
- Update OP_DISALLOWPUSHINPUTREF to push value to stack

## Usage recommendations

The release of Radiant Node 1.2.0 is available for everyone. Recommended to update as soon as possible (well before the November 17, 2022 activation date)

## Network changes

n/a

## Added functionality

### <refHash 32 bytes> OP_REFHASHVALUESUM_UTXOS (Renamed from OP_UTXOREFVALUESUM)

Pushes the total sum of photons across all of the input utxos by the 32 byte hash of the sorted set of ref asset ids. 

### <refHash 32 bytes> OP_REFHASHVALUESUM_OUTPUTS

Pushes the total sum of photons across all of the outputs by the 32 byte hash of the sorted set of ref asset ids. 

### <inputIndex> OP_REFHASHDATASUMMARY_UTXO (Renamed from OP_UTXODATASUMMARY)

Pushes a vector of the form onto the stack from an inputIndex.

`hash256(<nValue><hash256(scriptPubKey)><numRefs><hash(sortedMap(pushRefs))>)`

This allows an unlocking context to access any other input's scriptPubKey and determine what 'type' it is and all other details of that script.

### <outputIndex> OP_REFHASHDATASUMMARY_OUTPUT

Same functionality as OP_REFHASHDATASUMMARY_UTXO, but for outputs.

### OP_PUSHINPUTREFSINGLETON

Functionally the same as combining OP_PUSHINPUTREF and OP_DISALLOWPUSHINPUTREFSIBLING, but also the added restriction that an OP_PUSHINPUTREFSINGLETON
must originate from another OP_PUSHINPUTREFSINGLETON. This enables polymorphic NFTs with almost zero code.

### <refAssetId 36 bytes> OP_REFTYPE_UTXO

Given a reference asset id, determine the type of reference used in any input utxo. NOT_FOUND=0, NORMAL_REF=1, SINGLETON_REF=2

### <refAssetId 36 bytes> OP_REFTYPE_OUTPUT

Same as OP_REFTYPE_UTXO, but for the outputs.
 
### OP_STATESEPARATOR

Allow up to a single OP_STATESEPARATOR in a script. When used, no OP_RETURN is allowed in the same script.
The OP_STATESEPARATOR is interpretted as a NOP, however it allows a demarcation point for calculating a `codeScriptHash`
which begins from the first byte=0 (when OP_STATESEPARATOR is not used), but starts from the byte location of the 
OP_STATESEPARATOR when it is present. 

### <inputIndex> OP_STATESEPARATORINDEX_UTXO

Pushes 0 onto the stack or the byte index of the OP_STATESEPARATOR (if present) in the input utxo at the inputIndex.

### <outputIndex> OP_STATESEPARATORINDEX_OUTPUT

Same as OP_STATESEPARATORINDEX_UTXO, but for the outputs.

### <refAssetId 36 bytes> OP_REFVALUESUM_UTXOS

Pushes the total sum of photons across all of the input utxos by the 36 byte ref asset id.

### <refAssetId 36 bytes> OP_REFVALUESUM_OUTPUTS

Same as OP_REFVALUESUM_UTXOS, but for the outputs.

### <refAssetId 36 bytes> OP_REFOUTPUTCOUNT_UTXOS

Pushes the total number of input utxos that contain at least one usage of a ref asset id.

### <refAssetId 36 bytes> OP_REFOUTPUTCOUNT_OUTPUTS

Same as OP_REFOUTPUTCOUNT_UTXOS, but for the outputs.

### <refAssetId 36 bytes> OP_REFOUTPUTCOUNTZEROVALUED_UTXOS

Pushes the total number of input utxos that contain at least one usage of a ref asset id and the value of the input utxo is 0-sats.

### <refAssetId 36 bytes> OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS

Same as OP_REFOUTPUTCOUNTZEROVALUED_UTXOS, but for the outputs.
 
### <inputIndex> OP_REFDATASUMMARY_UTXO

Pushes a vector of the form onto the stack from an inputIndex.

`<refAssetId1 36 bytes><refAssetId2 36 bytes><refAssetId2 36 bytes>...`

If an input utxo does not contain at least one reference, then the 36 byte zeroes are pushed. 
This OP code can be used to identify input utxos that have no reference, and also be able to identify the precise
references. To calculate the number of references, take the len() of the vector and divide by 36 and check it is not equal to 36 byte zeroes.

### <outputIndex> OP_REFDATASUMMARY_OUTPUT

Same as OP_REFDATASUMMARY_UTXO, but for the outputs.

### <codeScriptHash 32 bytes> OP_CODESCRIPTHASHVALUESUM_UTXOS

Pushes the total sum of photons across all of the input utxos by the codeScriptHash.

### <codeScriptHash 32 bytes> OP_CODESCRIPTHASHVALUESUM_OUTPUTS

Same as OP_REFVALUESUM_UTXOS, but for the outputs.

### <codeScriptHash 32 bytes> OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS

Pushes the total number of input utxos that contain at least one usage of a ref asset id.

### <codeScriptHash 32 bytes> OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS

Same as OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS, but for the outputs.

### <codeScriptHash 32 bytes> OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS

Pushes the total number of input utxos that contain at least one usage of a ref asset id and the value of the input utxo is 0-sats.

### <codeScriptHash 32 bytes> OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS

Same as OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS, but for the outputs.

### <inputIndex> OP_CODESCRIPTBYTECODE_UTXO

Pushes the part of the script after the OP_STATESEPERATOR (or starting from the beginning if there was none for an input utxo by inputIndex.

### <outputIndex> OP_CODESCRIPTBYTECODE_OUTPUT

Same as OP_CODESCRIPTBYTECODE_UTXO, but for the outputs.
  
### <inputIndex> OP_STATESCRIPTBYTECODE_UTXO

Pushes the part of the bytecode from the beginning of a script until the byte right before the use of OP_STATESEPARATOR.
If there is no OP_STATESEPARATOR or it appears at the very first byte in a script, then then null 0x00 value is pushed to the stack.

### <outputIndex> OP_STATESCRIPTBYTECODE_OUTPUT

Same as OP_STATESCRIPTBYTECODE_UTXO but for the outputs.
 
## Deprecated functionality

n/a

## Modified functionality

- Increased default excessive block size from 128 MB to 256 MB.
- Fixed printing of script to ASM
- Removed validating induction OP codes on rolling a block forward as the blocks were already validated, and the extra overhead is not needed
- Correctly print "Radiant server starting" on startup

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

## Changes since Radiant Node 1.1.3

- See above.

### New documents

n/a

### Removed documents

n/a

### Notable commits grouped by functionality

#### Security or consensus relevant fixes

The SCRIPT_ENHANCED_REFERENCES flag becomes active after block height 62,000 (ConsensusParams.ERHeight).

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
