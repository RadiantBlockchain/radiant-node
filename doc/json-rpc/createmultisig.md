`createmultisig` JSON-RPC command
=================================

**`createmultisig nrequired ["key",...]`**

```
Creates a multi-signature address with n signature of m keys required.
It returns a json object with the address and redeemScript.
```

Arguments
---------

```
1. nrequired      (numeric, required) The number of required signatures out of the n keys.
2. "keys"       (string, required) A json array of hex-encoded public keys
     [
       "key"    (string) The hex-encoded public key
       ,...
     ]
```

Result
------

```
{
  "address":"multisigaddress",  (string) The value of the new multisig address.
  "redeemScript":"script"       (string) The string value of the hex-encoded redemption script.
}
```

Examples
--------

```
Create a multisig address from 2 public keys
> bitcoin-cli createmultisig 2 "[\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\",\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\"]"

As a JSON-RPC call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "createmultisig", "params": [2, "[\"03789ed0bb717d88f7d321a368d905e7430207ebbd82bd342cf11ae157a7ace5fd\",\"03dbc6764b8884a92e871274b87583e6d5c2a58819473e17e107ef3f6aa5a61626\"]"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
