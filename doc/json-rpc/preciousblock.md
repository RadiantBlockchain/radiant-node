`preciousblock` JSON-RPC command
================================

**`preciousblock "blockhash"`**

```
Treats a block as if it were received before others with the same work.

A later preciousblock call can override the effect of an earlier one.

The effects of preciousblock are not retained across restarts.
```

Arguments
---------

```
1. "blockhash"   (string, required) the hash of the block to mark as precious
```

Result
------

Examples
--------

```
> bitcoin-cli preciousblock "blockhash"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "preciousblock", "params": ["blockhash"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
