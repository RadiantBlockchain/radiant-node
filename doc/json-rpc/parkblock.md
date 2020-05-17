`parkblock` JSON-RPC command
============================

**`parkblock "blockhash"`**

```
Marks a block as parked.
```

Arguments
---------

```
1. "blockhash"   (string, required) the hash of the block to park
```

Result
------

Examples
--------

```
> bitcoin-cli parkblock "blockhash"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "parkblock", "params": ["blockhash"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
