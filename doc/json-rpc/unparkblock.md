`unparkblock` JSON-RPC command
==============================

**`unparkblock "blockhash"`**

```
Removes parked status of a block and its descendants, reconsider them for activation.
This can be used to undo the effects of parkblock.
```

Arguments
---------

```
1. "blockhash"   (string, required) the hash of the block to unpark
```

Result
------

Examples
--------

```
> bitcoin-cli unparkblock "blockhash"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "unparkblock", "params": ["blockhash"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
