# Bitcoin Cash Node bitcoin-tx utility version v0.21.2

| Usage                                      | Description                            |
| :----------------------------------------- | :------------------------------------- |
| `bitcoin-tx [options] <hex-tx> [commands]` | Update hex-encoded bitcoin transaction |
| `bitcoin-tx [options] -create [commands]`  | Create hex-encoded bitcoin transaction |

Options
-------

| Argument            | Description                                                                                                                                      |
| :------------------ | :----------------------------------------------------------------------------------------------------------------------------------------------- |
| `-?`, `-h`, `-help` | Print this help message and exit                                                                                                                 |
| `-create`           | Create new, empty TX.                                                                                                                            |
| `-json`             | Select JSON output                                                                                                                               |
| `-txid`             | Output only the hex-encoded transaction id of the resultant transaction.                                                                         |
| `-usecashaddr`      | In JSON output, use CashAddr address format for destination encoding instead of the legacy base58 format (default: 0, will change to 1 in v0.22) |

Chain selection options
-----------------------

| Argument   | Description                                                                                                                                                        |
| :--------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `-regtest` | Enter regression test mode, which uses a special chain in which blocks can be solved instantly. This is intended for regression testing tools and app development. |
| `-testnet` | Use the test chain                                                                                                                                                 |

Commands
--------

| Argument                                                          | Description                                                                                                                                                                                                  |
| :---------------------------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `delin=N`                                                         | Delete input N from TX                                                                                                                                                                                       |
| `delout=N`                                                        | Delete output N from TX                                                                                                                                                                                      |
| `in=TXID:VOUT(:SEQUENCE_NUMBER)`                                  | Add input to TX                                                                                                                                                                                              |
| `locktime=N`                                                      | Set TX lock time to N                                                                                                                                                                                        |
| `nversion=N`                                                      | Set TX version to N                                                                                                                                                                                          |
| `outaddr=VALUE:ADDRESS`                                           | Add address-based output to TX                                                                                                                                                                               |
| `outdata=[VALUE:]DATA`                                            | Add data-based output to TX                                                                                                                                                                                  |
| `outmultisig=VALUE:REQUIRED:PUBKEYS:PUBKEY1:PUBKEY2:....[:FLAGS]` | Add Pay To n-of-m Multi-sig output to TX. n = REQUIRED, m = PUBKEYS. Optionally add the "S" flag to wrap the output in a pay-to-script-hash.                                                                 |
| `outpubkey=VALUE:PUBKEY[:FLAGS]`                                  | Add pay-to-pubkey output to TX. Optionally add the "S" flag to wrap the output in a pay-to-script-hash.                                                                                                      |
| `outscript=VALUE:SCRIPT[:FLAGS]`                                  | Add raw script output to TX. Optionally add the "S" flag to wrap the output in a pay-to-script-hash.                                                                                                         |
| `sign=SIGHASH-FLAGS`                                              | Add zero or more signatures to transaction. This command requires JSON registers:prevtxs=JSON object, privatekeys=JSON object. See signrawtransactionwithkey docs for format of sighash flags, JSON objects. |

Register Commands
-----------------

| Argument               | Description                                |
| :--------------------- | :----------------------------------------- |
| `load=NAME:FILENAME`   | Load JSON file FILENAME into register NAME |
| `set=NAME:JSON-STRING` | Set register NAME to given JSON-STRING     |
