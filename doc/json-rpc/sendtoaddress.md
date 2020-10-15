`sendtoaddress` JSON-RPC command
================================

**`sendtoaddress "address" amount ( "comment" "comment_to" subtractfeefromamount coinsel)`**

```
Send an amount to a given address.
```

Arguments
---------

```
1. "address"            (string, required) The bitcoin address to send to.
2. "amount"             (numeric or string, required) The amount in BCH to send. eg 0.1
3. "comment"            (string, optional) A comment used to store what the transaction is for.
                             This is not part of the transaction, just kept in your wallet.
4. "comment_to"         (string, optional) A comment to store the name of the person or organization
                             to which you're sending the transaction. This is not part of the
                             transaction, just kept in your wallet.
5. subtractfeefromamount  (boolean, optional, default=false) The fee will be deducted from the amount being sent.
                             The recipient will receive less bitcoins than you enter in the amount field.
6. coinsel  (int, optional, default=0) Which coin selection
algorithm to use. A value of 1 will use a faster algorithm suitable for stress tests or use with large wallets. This
algorithm is likely to produce larger transactions on average.0 is a slower algorithm using BNB and a knapsack solver, but
which can produce transactions with slightly better privacy and smaller transaction sizes. Values other than 0 or 1 are reserved
for future algorithms.
```

Result
------

```
"txid"                  (string) The transaction id.
```

Examples
--------

```
> bitcoin-cli sendtoaddress "1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd" 0.1
> bitcoin-cli sendtoaddress "1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd" 0.1 "donation" "seans outpost"
> bitcoin-cli sendtoaddress "1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd" 0.1 "" "" true
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "sendtoaddress", "params": ["1M72Sfpbz1BPpXFHz9m3CdqATR44Jvaydd", 0.1, "donation", "seans outpost"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
