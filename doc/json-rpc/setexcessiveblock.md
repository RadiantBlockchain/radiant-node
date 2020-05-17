`setexcessiveblock` JSON-RPC command
====================================

**`setexcessiveblock blockSize`**

```
Set the excessive block size. Excessive blocks will not be used in the active chain or relayed. This  discourages the propagation of blocks that you consider excessively large.
Result
  blockSize (integer) excessive block size in bytes
```

Examples
--------

```
> bitcoin-cli setexcessiveblock 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "setexcessiveblock", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
