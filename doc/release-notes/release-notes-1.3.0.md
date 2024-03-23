# Release Notes for Radiant Node Version 1.3.0 (Energy)

Release Name: Energy 

Radiant Node version 1.3.0 (Energy) is now available from: <https://radiantblockchain.org>

## Overview

Add additional transaction introspection OP codes.

See detailed description of the OP codes below and their capabilities.

List of new OP Codes:

- `<value> OP_PUSH_TX_STATE`
0: Current transaction id
1: Total input photons spent
2: Total output photons
 
Additional bug fixes and improvements:

- Correctly validate the OP_DISALLOWPUSHINPUTREF is treated correctly
- Fix build error on Arch linux
- Fix build error for GCC 13

## Usage recommendations

The release of Radiant Node 1.3.0 is available for everyone. 
Recommended to update as soon as possible (well before block height 214,555 ~ May 1, 2024)

## Network changes

n/a

## Added functionality

### <1 byte> OP_PUSH_TX_STATE 

Pushes information about the current transaction onto the stack. Accepts 1 byte which determines the data to push.

0: Current transaction id
1: Total input photons spent
2: Total output photons
  
## Deprecated functionality

n/a

## Modified functionality

- Updated script interpretor to correctly handle OP_DISALLOWPUSHINPUTREF

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

## Changes since Radiant Node 1.2.0

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
