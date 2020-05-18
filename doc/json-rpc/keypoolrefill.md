`keypoolrefill` JSON-RPC command
================================

**`keypoolrefill ( newsize )`**

```
Fills the keypool.

Arguments
1. newsize     (numeric, optional, default=100) The new keypool size
```

Examples
--------

```
> bitcoin-cli keypoolrefill 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "keypoolrefill", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
