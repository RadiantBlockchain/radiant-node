`getblockcount` JSON-RPC command
================================

**`getblockcount`**

```
Returns the number of blocks in the longest blockchain.
```

Result
------

```
n    (numeric) The current block count
```

Examples
--------

```
> bitcoin-cli getblockcount 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblockcount", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
