`finalizeblock` JSON-RPC command
================================

**`finalizeblock "blockhash"`**

```
Treats a block as final. It cannot be reorged. Any chain
that does not contain this block is invalid. Used on a less
work chain, it can effectively PUTS YOU OUT OF CONSENSUS.
USE WITH CAUTION!
```

Result
------

Examples
--------

```
> bitcoin-cli finalizeblock "blockhash"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "finalizeblock", "params": ["blockhash"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
