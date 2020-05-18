`addmultisigaddress` JSON-RPC command
=====================================

**`addmultisigaddress nrequired ["key",...] ( "label" )`**

```
Add a nrequired-to-sign multisignature address to the wallet. Requires a new wallet backup.
Each key is a Bitcoin address or hex-encoded public key.
If 'label' is specified (DEPRECATED), assign address to that label.
```

Arguments
---------

```
1. nrequired        (numeric, required) The number of required signatures out of the n keys or addresses.
2. "keys"         (string, required) A json array of bitcoin addresses or hex-encoded public keys
     [
       "address"  (string) bitcoin address or hex-encoded public key
       ...,
     ]
3. "label"                        (string, optional) A label to assign the addresses to.
```

Result
------

```
{
  "address":"multisigaddress",    (string) The value of the new multisig address.
  "redeemScript":"script"         (string) The string value of the hex-encoded redemption script.
}
```

Examples
--------

```
Add a multisig address from 2 addresses
> bitcoin-cli addmultisigaddress 2 "[\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\",\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\"]"

As a JSON-RPC call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "addmultisigaddress", "params": [2, "[\"16sSauSf5pF2UkUwvKGq4qjNRzBZYqgEL5\",\"171sgjn4YtPu27adkKGrdDwzRTxnRkBfKV\"]"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
