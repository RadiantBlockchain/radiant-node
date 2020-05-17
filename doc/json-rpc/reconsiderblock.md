`reconsiderblock` JSON-RPC command
==================================

**`reconsiderblock "blockhash"`**

```
Removes invalidity status of a block and its descendants, reconsider them for activation.
This can be used to undo the effects of invalidateblock.
```

Arguments
---------

```
1. "blockhash"   (string, required) the hash of the block to reconsider
```

Result
------

Examples
--------

```
> bitcoin-cli reconsiderblock "blockhash"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "reconsiderblock", "params": ["blockhash"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
