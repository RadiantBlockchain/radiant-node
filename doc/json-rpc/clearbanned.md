`clearbanned` JSON-RPC command
==============================

**`clearbanned ( manual automatic )`**

```
Clear all banned and/or discouraged IPs.
```

Arguments
---------

```
1. "manual"       (boolean, optional) true to clear all manual bans, false to not clear them. (default = true)
2. "automatic"    (boolean, optional) true to clear all automatic discouragements, false to not clear them. (default = true)
```

Examples
--------

```
> bitcoin-cli clearbanned
> bitcoin-cli clearbanned true
> bitcoin-cli clearbanned true false
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "clearbanned", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "clearbanned", "params": [false, true] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.2.0*
