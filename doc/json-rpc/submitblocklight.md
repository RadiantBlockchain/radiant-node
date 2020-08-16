`submitblocklight` JSON-RPC command
===================================

**`submitblocklight "hexdata" "job_id"`**

```
Attempts to submit a new block to the network, based on a previous call to getblocktemplatelight.

Arguments
1. "hexdata"        (string, required) The hex-encoded block data to submit. The block must have exactly 1 transaction (coinbase). Additional transactions (if any) are appended from the light template.
2. "job_id"         (string, required) Identifier of the light template from which to retrieve the non-coinbase transactions. This job_id must be obtained from a previous call to getblocktemplatelight.
```

Result
------

Examples
--------

```
> bitcoin-cli submitblocklight "mydata" "myjobid"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "submitblocklight", "params": ["mydata", "myjobid"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
