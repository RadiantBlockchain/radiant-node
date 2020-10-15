`listunspent` JSON-RPC command
==============================

**`listunspent ( minconf maxconf ["address",...] include_unsafe {"minimumAmount":amount,"maximumAmount":amount,"maximumCount":n,"minimumSumAmount":amount} )`**

```
Returns array of unspent transaction outputs
with between minconf and maxconf (inclusive) confirmations.
Optionally filter to only include txouts paid to specified addresses.
```

Arguments
---------

```
1. minconf          (numeric, optional, default=1) The minimum confirmations to filter
2. maxconf          (numeric, optional, default=9999999) The maximum confirmations to filter
3. "addresses"      (string, optional) A json array of bitcoin addresses to filter
    [
      "address"     (string) bitcoin address
      ,...
    ]
4. include_unsafe (bool, optional, default=true) Include outputs that are not safe to spend
                  See description of "safe" attribute below.
5. query_options    (json, optional) JSON with query options
    {
      "minimumAmount"    (numeric or string, default=0) Minimum value of each UTXO in BCH
      "maximumAmount"    (numeric or string, default=unlimited) Maximum value of each UTXO in BCH
      "maximumCount"     (numeric or string, default=unlimited) Maximum number of UTXOs
      "minimumSumAmount" (numeric or string, default=unlimited) Minimum sum value of all UTXOs in BCH
    }

Result
[                   (array of json object)
  {
    "txid" : "txid",          (string) the transaction id
    "vout" : n,               (numeric) the vout value
    "address" : "address",    (string) the bitcoin address
    "label" : "label",        (string) The associated label, or "" for the default label
    "scriptPubKey" : "key",   (string) the script key
    "amount" : x.xxx,         (numeric) the transaction output amount in BCH
    "confirmations" : n,      (numeric) The number of confirmations
    "redeemScript" : n        (string) The redeemScript if scriptPubKey is P2SH
    "spendable" : xxx,        (bool) Whether we have the private keys to spend this output
    "solvable" : xxx,         (bool) Whether we know how to spend this output, ignoring the lack of keys
    "safe" : xxx              (bool) Whether this output is considered safe to spend. Unconfirmed transactions
                              from outside keys are considered unsafe and are not eligible for spending by
                              fundrawtransaction and sendtoaddress.
  }
  ,...
]

Examples
> bitcoin-cli listunspent
> bitcoin-cli listunspent 6 9999999 "[\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\",\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\"]"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "listunspent", "params": [6, 9999999 "[\"1PGFqEzfmQch1gKD3ra4k18PNj3tTUUSqg\",\"1LtvqCaApEdUGFkpKMM4MstjcaL4dKg8SP\"]"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
> bitcoin-cli listunspent 6 9999999 '[]' true '{ "minimumAmount": 0.005 }'
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "listunspent", "params": [6, 9999999, [] , true, { "minimumAmount": 0.005 } ] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
