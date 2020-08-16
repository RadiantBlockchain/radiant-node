`listwalletdir` JSON-RPC command
================================

**`listwalletdir`**

```
Returns a list of wallets in the wallet directory.
{
  "wallets" : [                (json array of objects)
    {
      "name" : "name"          (string) The wallet name
    }
    ,...
  ]
}
```

Examples
--------

```
> bitcoin-cli listwalletdir
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "listwalletdir", "params": [] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.0.0*
