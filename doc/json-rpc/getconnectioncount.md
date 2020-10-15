`getconnectioncount` JSON-RPC command
=====================================

**`getconnectioncount`**

```
Returns the number of connections to other nodes.
```

Result
------

```
n          (numeric) The connection count
```

Examples
--------

```
> bitcoin-cli getconnectioncount
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getconnectioncount", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
