`converttopsbt` JSON-RPC command
================================

**`converttopsbt "hexstring" ( permitsigdata )`**

```
Converts a network serialized transaction to a PSBT. This should be used only with createrawtransaction and fundrawtransaction
createpsbt and walletcreatefundedpsbt should be used for new applications.
```

Arguments
---------

```
1. "hexstring"              (string, required) The hex string of a raw transaction
2. permitsigdata           (boolean, optional, default=false) If true, any signatures in the input will be discarded and conversion.
                              will continue. If false, RPC will fail if any signatures are present.
```

Result
------

```
  "psbt"        (string)  The resulting raw transaction (base64-encoded string)
```

Examples
--------

```
Create a transaction
> bitcoin-cli createrawtransaction "[{\"txid\":\"myid\",\"vout\":0}]" "[{\"data\":\"00010203\"}]"

Convert the transaction to a PSBT
> bitcoin-cli converttopsbt "rawtransaction"
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
