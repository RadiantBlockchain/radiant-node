`abortrescan` JSON-RPC command
==============================

**`abortrescan`**

```
Stops current wallet rescan triggered by an RPC call, e.g. by an importprivkey call.
```

Examples
--------

```
Import a private key
> bitcoin-cli importprivkey "mykey"

Abort the running wallet rescan
> bitcoin-cli abortrescan

As a JSON-RPC call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "abortrescan", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
