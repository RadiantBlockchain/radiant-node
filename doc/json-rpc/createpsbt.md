`createpsbt` JSON-RPC command
=============================

**`createpsbt [{"txid":"hex","vout":n,"sequence":n},...] [{"address":amount},{"data":"hex"},...] ( locktime )`**

```
Creates a transaction in the Partially Signed Transaction format.
Implements the Creator role.
```

Arguments
---------

```
1. "inputs"                (array, required) A json array of json objects
     [
       {
         "txid":"id",      (string, required) The transaction id
         "vout":n,         (numeric, required) The output number
         "sequence":n      (numeric, optional) The sequence number
       } 
       ,...
     ]
2. "outputs"               (array, required) a json array with outputs (key-value pairs)
   [
    {
      "address": x.xxx,    (obj, optional) A key-value pair. The key (string) is the bitcoin address, the value (float or string) is the amount in BCH
    },
    {
      "data": "hex"        (obj, optional) A key-value pair. The key must be "data", the value is hex-encoded data
    }
    ,...                     More key-value pairs of the above form. For compatibility reasons, a dictionary, which holds the key-value pairs directly, is also
                             accepted as second parameter.
   ]
3. locktime                  (numeric, optional, default=0) Raw locktime. Non-0 value also locktime-activates inputs
```

Result
------

```
  "psbt"        (string)  The resulting raw transaction (base64-encoded string)
```

Examples
--------

```
> bitcoin-cli createpsbt "[{\"txid\":\"myid\",\"vout\":0}]" "[{\"data\":\"00010203\"}]"
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
