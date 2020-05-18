`listtransactions` JSON-RPC command
===================================

**`listtransactions ( "label" count skip include_watchonly )`**

```
If a label name is provided, this will return only incoming transactions paying to addresses with the specified label.

Returns up to 'count' most recent transactions skipping the first 'from' transactions.
```

Arguments
---------

```
1. "label"    (string, optional) If set, should be a valid label name to return only incoming transactions with the specified label, or "*" to disable filtering and return all transactions.
2. count          (numeric, optional, default=10) The number of transactions to return
3. skip           (numeric, optional, default=0) The number of transactions to skip
4. include_watchonly (bool, optional, default=false) Include transactions to watch-only addresses (see 'importaddress')
```

Result
------

```
[
  {
    "address":"address",    (string) The bitcoin address of the transaction.
    "category":"send|receive", (string) The transaction category.
    "amount": x.xxx,          (numeric) The amount in BCH. This is negative for the 'send' category, and is positive
                                        for the 'receive' category,
    "label": "label",       (string) A comment for the address/transaction, if any
    "vout": n,                (numeric) the vout value
    "fee": x.xxx,             (numeric) The amount of the fee in BCH. This is negative and only available for the 
                                         'send' category of transactions.
    "confirmations": n,       (numeric) The number of confirmations for the transaction. Negative confirmations indicate the
                                         transaction conflicts with the block chain
    "trusted": xxx,           (bool) Whether we consider the outputs of this unconfirmed transaction safe to spend.
    "blockhash": "hashvalue", (string) The block hash containing the transaction.
    "blockindex": n,          (numeric) The index of the transaction in the block that includes it.
    "blocktime": xxx,         (numeric) The block time in seconds since epoch (1 Jan 1970 GMT).
    "txid": "transactionid", (string) The transaction id.
    "time": xxx,              (numeric) The transaction time in seconds since epoch (midnight Jan 1 1970 GMT).
    "timereceived": xxx,      (numeric) The time received in seconds since epoch (midnight Jan 1 1970 GMT).
    "comment": "...",       (string) If a comment is associated with the transaction.
    "abandoned": xxx          (bool) 'true' if the transaction has been abandoned (inputs are respendable). Only available for the 
                                         'send' category of transactions.
  }
]
```

Examples
--------

```
List the most recent 10 transactions in the systems
> bitcoin-cli listtransactions 

List transactions 100 to 120
> bitcoin-cli listtransactions "*" 20 100

As a JSON-RPC call
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "listtransactions", "params": ["*", 20, 100] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
