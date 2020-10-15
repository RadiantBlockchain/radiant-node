`setban` JSON-RPC command
=========================

**`setban "subnet" "command" ( bantime absolute )`**

```
Attempts to add or remove a IP/Subnet from the banned list.
Peers that are automatically banned cannot be unbanned.
```

Arguments
---------

```
1. "subnet"       (string, required) The IP/Subnet (see getpeerinfo for nodes IP) with an optional netmask (default is /32 = single IP)
2. "command"      (string, required) 'add' to add an IP/Subnet to the list, 'remove' to remove an IP/Subnet from the list
3. "bantime"      (numeric, optional) time in seconds how long (or until when if [absolute] is set) the IP is banned (0 or empty means using the default time of 24h which can also be overwritten by the -bantime startup argument)
4. "absolute"     (boolean, optional) If set, the bantime must be an absolute timestamp in seconds since epoch (Jan 1 1970 GMT)
```

Examples
--------

```
> bitcoin-cli setban "192.168.0.6" "add" 86400
> bitcoin-cli setban "192.168.0.0/24" "add"
> curl --user myusername --data-binary '{"jsonrpc": "1.0", "id":"curltest", "method": "setban", "params": ["192.168.0.6", "add", 86400] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/
```

***

*Bitcoin Cash Node Daemon version v22.1.0*
