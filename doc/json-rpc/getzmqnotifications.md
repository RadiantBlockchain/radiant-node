`getzmqnotifications` JSON-RPC command
======================================

**`getzmqnotifications`**

```
Returns information about the active ZeroMQ notifications.
```

Result
------

```
[
  {                        (json object)
    "type": "pubhashtx",   (string) Type of notification
    "address": "..."       (string) Address of the publisher
  },
  ...
]
```

Examples
--------

```
> bitcoin-cli getzmqnotifications 
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getzmqnotifications", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v0.21.2*
