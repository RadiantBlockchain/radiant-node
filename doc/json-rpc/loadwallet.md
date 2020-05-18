`loadwallet` JSON-RPC command
=============================

**`loadwallet "filename"`**

```
Loads a wallet from a wallet file or directory.
Note that all wallet command-line options used when starting bitcoind will be
applied to the new wallet (eg -zapwallettxes, upgradewallet, rescan, etc).
```

Arguments
---------

```
1. "filename"    (string, required) The wallet directory or .dat file.
```

Result
------

```
{
  "name" :    <wallet_name>,        (string) The wallet name if loaded successfully.
  "warning" : <warning>,            (string) Warning message if wallet was not loaded cleanly.
}
```

Examples
--------

```
> bitcoin-cli loadwallet "test.dat"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "loadwallet", "params": ["test.dat"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
