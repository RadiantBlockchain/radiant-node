`clearbanned` JSON-RPC command
==============================

**`clearbanned`**

```
Clear all banned IPs.
```

Examples
--------

```
> bitcoin-cli clearbanned 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "clearbanned", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
