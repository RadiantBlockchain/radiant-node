`submitheader` JSON-RPC command
===============================

**`submitheader "hexdata"`**

```
Decode the given hexdata as a header and submit it as a candidate chain tip if valid.
Throws when the header is invalid.

Arguments
1. "hexdata"        (string, required) the hex-encoded block header data
```

Result
------

```
None
Examples:
> bitcoin-cli submitheader "aabbcc"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "submitheader", "params": ["aabbcc"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
