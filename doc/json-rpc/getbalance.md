`getbalance` JSON-RPC command
=============================

**`getbalance ( "dummy" minconf include_watchonly )`**

```
Returns the total available balance.
The available balance is what the wallet considers currently spendable, and is
thus affected by options which limit spendability such as -spendzeroconfchange.
```

Arguments
---------

```
1. (dummy)           (string, optional) Remains for backward compatibility. Must be excluded or set to "*".
2. minconf           (numeric, optional, default=0) Only include transactions confirmed at least this many times.
3. include_watchonly (bool, optional, default=false) Also include balance in watch-only addresses (see 'importaddress')
```

Result
------

```
amount              (numeric) The total amount in BCH received for this wallet.
```

Examples
--------

```
The total amount in the wallet with 1 or more confirmations
> bitcoin-cli getbalance

The total amount in the wallet at least 6 blocks confirmed
> bitcoin-cli getbalance "*" 6

As a JSON-RPC call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getbalance", "params": ["*", 6] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.2.0*
