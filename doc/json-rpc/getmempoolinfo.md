`getmempoolinfo` JSON-RPC command
=================================

**`getmempoolinfo`**

```
Returns details on the active state of the TX memory pool.
```

Result
------

```
{
  "loaded": true|false         (boolean) True if the mempool is fully loaded
  "size": xxxxx,               (numeric) Current tx count
  "bytes": xxxxx,              (numeric) Transaction size.
  "usage": xxxxx,              (numeric) Total memory usage for the mempool
  "maxmempool": xxxxx,         (numeric) Maximum memory usage for the mempool
  "mempoolminfee": xxxxx       (numeric) Minimum fee rate in BCH/kB for tx to be accepted. Is the maximum of minrelaytxfee and minimum mempool fee
  "minrelaytxfee": xxxxx       (numeric) Current minimum relay fee for transactions
}
```

Examples
--------

```
> bitcoin-cli getmempoolinfo 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getmempoolinfo", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
