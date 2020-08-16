`gettxoutsetinfo` JSON-RPC command
==================================

**`gettxoutsetinfo`**

```
Returns statistics about the unspent transaction output set.
Note this call may take some time.
```

Result
------

```
{
  "height":n,     (numeric) The current block height (index)
  "bestblock": "hex",   (string) the best block hash hex
  "transactions": n,      (numeric) The number of transactions
  "txouts": n,            (numeric) The number of output transactions
  "bogosize": n,          (numeric) A database-independent metric for UTXO set size
  "hash_serialized": "hash",   (string) The serialized hash
  "disk_size": n,         (numeric) The estimated size of the chainstate on disk
  "total_amount": x.xxx          (numeric) The total amount
}
```

Examples
--------

```
> bitcoin-cli gettxoutsetinfo
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "gettxoutsetinfo", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
