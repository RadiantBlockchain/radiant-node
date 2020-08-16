`listbanned` JSON-RPC command
=============================

**`listbanned`**

```
List all manually banned IPs/Subnets.
```

Examples
--------

```
> bitcoin-cli listbanned
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "listbanned", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
