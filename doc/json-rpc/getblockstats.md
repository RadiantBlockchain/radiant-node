`getblockstats` JSON-RPC command
================================

**`getblockstats hash_or_height ( stats )`**

```
Compute per block statistics for a given window. All amounts are in BCH.
It won't work for some heights with pruning.
It won't work without -txindex for utxo_size_inc, *fee or *feerate stats.
```

Arguments
---------

```
1. "hash_or_height"     (string or numeric, required) The block hash or height of the target block
2. "stats"              (array,  optional) Values to plot, by default all values (see result below)
    [
      "height",         (string, optional) Selected statistic
      "time",           (string, optional) Selected statistic
      ,...
    ]
```

Result
------

```
{                           (json object)
  "avgfee": x.xxx,          (numeric) Average fee in the block
  "avgfeerate": x.xxx,      (numeric) Average feerate (in BCH per byte)
  "avgtxsize": xxxxx,       (numeric) Average transaction size
  "blockhash": xxxxx,       (string) The block hash (to check for potential reorgs)
  "feerate_percentiles": [  (array of numeric) Feerates at the 10th, 25th, 50th, 75th, and 90th percentile weight unit (in satoshis per byte)
      "10th_percentile_feerate",      (numeric) The 10th percentile feerate
      "25th_percentile_feerate",      (numeric) The 25th percentile feerate
      "50th_percentile_feerate",      (numeric) The 50th percentile feerate
      "75th_percentile_feerate",      (numeric) The 75th percentile feerate
      "90th_percentile_feerate",      (numeric) The 90th percentile feerate
  ],
  "height": xxxxx,          (numeric) The height of the block
  "ins": xxxxx,             (numeric) The number of inputs (excluding coinbase)
  "maxfee": xxxxx,          (numeric) Maximum fee in the block
  "maxfeerate": xxxxx,      (numeric) Maximum feerate (in BCH per byte)
  "maxtxsize": xxxxx,       (numeric) Maximum transaction size
  "medianfee": x.xxx,       (numeric) Truncated median fee in the block
  "mediantime": xxxxx,      (numeric) The block median time past
  "mediantxsize": xxxxx,    (numeric) Truncated median transaction size
  "minfee": x.xxx,          (numeric) Minimum fee in the block
  "minfeerate": xx.xx,      (numeric) Minimum feerate (in BCH per byte)
  "mintxsize": xxxxx,       (numeric) Minimum transaction size
  "outs": xxxxx,            (numeric) The number of outputs
  "subsidy": x.xxx,         (numeric) The block subsidy
  "time": xxxxx,            (numeric) The block time
  "total_out": x.xxx,       (numeric) Total amount in all outputs (excluding coinbase and thus reward [ie subsidy + totalfee])
  "total_size": xxxxx,      (numeric) Total size of all non-coinbase transactions
  "totalfee": x.xxx,        (numeric) The fee total
  "txs": xxxxx,             (numeric) The number of transactions (excluding coinbase)
  "utxo_increase": xxxxx,   (numeric) The increase/decrease in the number of unspent outputs
  "utxo_size_inc": xxxxx,   (numeric) The increase/decrease in size for the utxo index (not discounting op_return and similar)
}
```

Examples
--------

```
> bitcoin-cli getblockstats 1000 '["minfeerate","avgfeerate"]'
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblockstats", "params": [1000 '["minfeerate","avgfeerate"]'] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
