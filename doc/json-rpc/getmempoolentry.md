`getmempoolentry` JSON-RPC command
==================================

**`getmempoolentry txid`**

```
Returns mempool data for given transaction
```

Arguments
---------

```
1. "txid"                   (string, required) The transaction id (must be in mempool)
```

Result
------

```
{                           (json object)
    "size" : n,             (numeric) transaction size.
    "fee" : n,              (numeric) transaction fee in BCH(DEPRECATED)
    "modifiedfee" : n,      (numeric) transaction fee with fee deltas used for mining priority (DEPRECATED)
    "time" : n,             (numeric) local time transaction entered pool in seconds since 1 Jan 1970 GMT
    "height" : n,           (numeric) block height when transaction entered pool
    "descendantcount" : n,  (numeric) number of in-mempool descendant transactions (including this one)
    "descendantsize" : n,   (numeric) transaction size of in-mempool descendants (including this one)
    "descendantfees" : n,   (numeric) modified fees (see above) of in-mempool descendants (including this one) (DEPRECATED)
    "ancestorcount" : n,    (numeric) number of in-mempool ancestor transactions (including this one)
    "ancestorsize" : n,     (numeric) transaction size of in-mempool ancestors (including this one)
    "ancestorfees" : n,     (numeric) modified fees (see above) of in-mempool ancestors (including this one) (DEPRECATED)
    "fees" : {
        "base" : n,         (numeric) transaction fee in BCH
        "modified" : n,     (numeric) transaction fee with fee deltas used for mining priority in BCH
        "ancestor" : n,     (numeric) modified fees (see above) of in-mempool ancestors (including this one) in BCH
        "descendant" : n,   (numeric) modified fees (see above) of in-mempool descendants (including this one) in BCH
    }
    "depends" : [           (array) unconfirmed transactions used as inputs for this transaction
        "transactionid",    (string) parent transaction id
       ... ]
    "spentby" : [           (array) unconfirmed transactions spending outputs from this transaction
        "transactionid",    (string) child transaction id
       ... ]
}
```

Examples
--------

```
> bitcoin-cli getmempoolentry "mytxid"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getmempoolentry", "params": ["mytxid"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
