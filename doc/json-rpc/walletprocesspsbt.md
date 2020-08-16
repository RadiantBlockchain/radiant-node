`walletprocesspsbt` JSON-RPC command
====================================

**`walletprocesspsbt "psbt" ( sign "sighashtype" bip32derivs )`**

```
Update a PSBT with input information from our wallet and then sign inputs
that we can sign for.
```

Arguments
---------

```
1. "psbt"                      (string, required) The transaction base64 string
2. sign                          (boolean, optional, default=true) Also sign the transaction when updating
3. "sighashtype"            (string, optional, default=ALL|FORKID) The signature hash type to sign with if not specified by the PSBT. Must be one of
       "ALL|FORKID"
       "NONE|FORKID"
       "SINGLE|FORKID"
       "ALL|FORKID|ANYONECANPAY"
       "NONE|FORKID|ANYONECANPAY"
       "SINGLE|FORKID|ANYONECANPAY"
4. bip32derivs                    (boolean, optional, default=false) If true, includes the BIP 32 derivation paths for public keys if we know them
```

Result
------

```
{
  "psbt" : "value",          (string) The base64-encoded partially signed transaction
  "complete" : true|false,   (boolean) If the transaction has a complete set of signatures
  ]
}
```

Examples
--------

```
> bitcoin-cli walletprocesspsbt "psbt"
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
