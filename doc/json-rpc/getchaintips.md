`getchaintips` JSON-RPC command
===============================

**`getchaintips`**

```
Return information about all known tips in the block tree, including the main chain as well as orphaned branches.
```

Result
------

```
[
  {
    "height": xxxx,         (numeric) height of the chain tip
    "hash": "xxxx",         (string) block hash of the tip
    "branchlen": 0          (numeric) zero for main chain
    "status": "active"      (string) "active" for the main chain
  },
  {
    "height": xxxx,
    "hash": "xxxx",
    "branchlen": 1          (numeric) length of branch connecting the tip to the main chain
    "status": "xxxx"        (string) status of the chain (active, valid-fork, valid-headers, headers-only, invalid)
  }
]
Possible values for status:
1.  "invalid"               This branch contains at least one invalid block
2.  "parked"                This branch contains at least one parked block
3.  "headers-only"          Not all blocks for this branch are available, but the headers are valid
4.  "valid-headers"         All blocks are available for this branch, but they were never fully validated
5.  "valid-fork"            This branch is not part of the active chain, but is fully validated
6.  "active"                This is the tip of the active main chain, which is certainly valid
```

Examples
--------

```
> bitcoin-cli getchaintips 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getchaintips", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
