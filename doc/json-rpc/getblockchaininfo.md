`getblockchaininfo` JSON-RPC command
====================================

**`getblockchaininfo`**

```
Returns an object containing various state info regarding blockchain processing.
```

Result
------

```
{
  "chain": "xxxx",              (string) current network name as defined in BIP70 (main, test, regtest)
  "blocks": xxxxxx,             (numeric) the current number of blocks processed in the server
  "headers": xxxxxx,            (numeric) the current number of headers we have validated
  "bestblockhash": "...",       (string) the hash of the currently best block
  "difficulty": xxxxxx,         (numeric) the current difficulty
  "mediantime": xxxxxx,         (numeric) median time for the current best block
  "verificationprogress": xxxx, (numeric) estimate of verification progress [0..1]
  "initialblockdownload": xxxx, (bool) (debug information) estimate of whether this node is in Initial Block Download mode.
  "chainwork": "xxxx"           (string) total amount of work in active chain, in hexadecimal
  "size_on_disk": xxxxxx,       (numeric) the estimated size of the block and undo files on disk
  "pruned": xx,                 (boolean) if the blocks are subject to pruning
  "pruneheight": xxxxxx,        (numeric) lowest-height complete block stored (only present if pruning is enabled)
  "automatic_pruning": xx,      (boolean) whether automatic pruning is enabled (only present if pruning is enabled)
  "prune_target_size": xxxxxx,  (numeric) the target size used by pruning (only present if automatic pruning is enabled)
  "warnings" : "...",           (string) any network and blockchain warnings.
}
```

Examples
--------

```
> bitcoin-cli getblockchaininfo
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getblockchaininfo", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
