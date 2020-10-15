`setlabel` JSON-RPC command
===========================

**`setlabel "address" "label"`**

```
Sets the label associated with the given address.
```

Arguments
---------

```
1. "address"         (string, required) The bitcoin address to be associated with a label.
2. "label"           (string, required) The label to assign to the address.
```

Examples
--------

```
> bitcoin-cli setlabel "1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX" "tabby"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "setlabel", "params": ["1D1ZrZNe3JUo7ZycKEYQQiQAWd9y54F4XX", "tabby"] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
