`getnewaddress` JSON-RPC command
================================

**`getnewaddress ( "label" )`**

```
Returns a new Bitcoin address for receiving payments.
If 'label' is specified, it is added to the address book
so payments received with the address will be associated with 'label'.
```

Arguments
---------

```
1. "label"          (string, optional) The label name for the address to be linked to. If not provided, the default label "" is used. It can also be set to the empty string "" to represent the default label. The label does not need to exist, it will be created if there is no label by the given name.
```

Result
------

```
"address"    (string) The new bitcoin address
```

Examples
--------

```
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "getnewaddress", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
