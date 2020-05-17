`dumpwallet` JSON-RPC command
=============================

**`dumpwallet "filename"`**

```
Dumps all wallet keys in a human-readable format to a server-side file. This does not allow overwriting existing files.
Imported scripts are included in the dumpsfile, but corresponding addresses may not be added automatically by importwallet.
Note that if your wallet contains keys which are not derived from your HD seed (e.g. imported keys), these are not covered by
only backing up the seed itself, and must be backed up too (e.g. ensure you back up the whole dumpfile).
```

Arguments
---------

```
1. "filename"    (string, required) The filename with path (either absolute or relative to bitcoind)
```

Result
------

```
{                           (json object)
  "filename" : {        (string) The filename with full absolute path
}
```

Examples
--------

```
> bitcoin-cli dumpwallet "test"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "dumpwallet", "params": ["test"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
