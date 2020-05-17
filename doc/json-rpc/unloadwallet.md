`unloadwallet` JSON-RPC command
===============================

**`unloadwallet ( "wallet_name" )`**

```
Unloads the wallet referenced by the request endpoint otherwise unloads the wallet specified in the argument.
Specifying the wallet name on a wallet endpoint is invalid.
Arguments:
1. "wallet_name"    (string, optional) The name of the wallet to unload.
```

Examples
--------

```
> bitcoin-cli unloadwallet wallet_name
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "unloadwallet", "params": [wallet_name] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
