Bitcoin Cash Node JSON-RPC commands
===================================

## Blockchain

* [**`finalizeblock`**`  "blockhash"`](finalizeblock.md)
* [**`getbestblockhash`**](getbestblockhash.md)
* [**`getblock`**`  "blockhash" ( verbosity )`](getblock.md)
* [**`getblockchaininfo`**](getblockchaininfo.md)
* [**`getblockcount`**](getblockcount.md)
* [**`getblockhash`**`  height`](getblockhash.md)
* [**`getblockheader`**`  "blockhash" ( verbose )`](getblockheader.md)
* [**`getblockstats`**`  hash_or_height ( stats )`](getblockstats.md)
* [**`getchaintips`**](getchaintips.md)
* [**`getchaintxstats`**`  ( nblocks "blockhash" )`](getchaintxstats.md)
* [**`getdifficulty`**](getdifficulty.md)
* [**`getfinalizedblockhash`**](getfinalizedblockhash.md)
* [**`getmempoolancestors`**`  txid ( verbose )`](getmempoolancestors.md)
* [**`getmempooldescendants`**`  txid ( verbose )`](getmempooldescendants.md)
* [**`getmempoolentry`**`  txid`](getmempoolentry.md)
* [**`getmempoolinfo`**](getmempoolinfo.md)
* [**`getrawmempool`**`  ( verbose )`](getrawmempool.md)
* [**`gettxout`**`  "txid" n ( include_mempool )`](gettxout.md)
* [**`gettxoutproof`**`  ["txid",...] ( "blockhash" )`](gettxoutproof.md)
* [**`gettxoutsetinfo`**](gettxoutsetinfo.md)
* [**`invalidateblock`**`  "blockhash"`](invalidateblock.md)
* [**`parkblock`**`  "blockhash"`](parkblock.md)
* [**`preciousblock`**`  "blockhash"`](preciousblock.md)
* [**`pruneblockchain`**`  height`](pruneblockchain.md)
* [**`reconsiderblock`**`  "blockhash"`](reconsiderblock.md)
* [**`savemempool`**](savemempool.md)
* [**`scantxoutset`**`  "action" [scanobjects,...]`](scantxoutset.md)
* [**`unparkblock`**`  "blockhash"`](unparkblock.md)
* [**`verifychain`**`  ( checklevel nblocks )`](verifychain.md)
* [**`verifytxoutproof`**`  "proof"`](verifytxoutproof.md)

## Control

* [**`getmemoryinfo`**`  ("mode")`](getmemoryinfo.md)
* [**`getrpcinfo`**](getrpcinfo.md)
* [**`help`**`  ( "command" )`](help.md)
* [**`logging`**`  ( <include> <exclude> )`](logging.md)
* [**`stop`**](stop.md)
* [**`uptime`**](uptime.md)

## Generating

* [**`generate`**`  nblocks ( maxtries )`](generate.md)
* [**`generatetoaddress`**`  nblocks address (maxtries)`](generatetoaddress.md)

## Mining

* [**`getblocktemplate`**`  ( "template_request" )`](getblocktemplate.md)
* [**`getblocktemplatelight`**`  ( "template_request" "additional_txs" )`](getblocktemplatelight.md)
* [**`getmininginfo`**](getmininginfo.md)
* [**`getnetworkhashps`**`  ( nblocks height )`](getnetworkhashps.md)
* [**`prioritisetransaction`**`  "txid" dummy fee_delta`](prioritisetransaction.md)
* [**`submitblock`**`  "hexdata"  ( "dummy" )`](submitblock.md)
* [**`submitblocklight`**`  "hexdata" "job_id"`](submitblocklight.md)
* [**`submitheader`**`  "hexdata"`](submitheader.md)

## Network

* [**`addnode`**`  "node" "command"`](addnode.md)
* [**`clearbanned`**](clearbanned.md)
* [**`disconnectnode`**`  ( "address" nodeid )`](disconnectnode.md)
* [**`getaddednodeinfo`**`  ( "node" )`](getaddednodeinfo.md)
* [**`getconnectioncount`**](getconnectioncount.md)
* [**`getexcessiveblock`**](getexcessiveblock.md)
* [**`getnettotals`**](getnettotals.md)
* [**`getnetworkinfo`**](getnetworkinfo.md)
* [**`getnodeaddresses`**`  ( count )`](getnodeaddresses.md)
* [**`getpeerinfo`**](getpeerinfo.md)
* [**`listbanned`**](listbanned.md)
* [**`ping`**](ping.md)
* [**`setban`**`  "subnet" "command" ( bantime absolute )`](setban.md)
* [**`setexcessiveblock`**`  blockSize`](setexcessiveblock.md)
* [**`setnetworkactive`**`  state`](setnetworkactive.md)

## Rawtransactions

* [**`combinepsbt`**`  ["psbt",...]`](combinepsbt.md)
* [**`combinerawtransaction`**`  ["hexstring",...]`](combinerawtransaction.md)
* [**`converttopsbt`**`  "hexstring" ( permitsigdata )`](converttopsbt.md)
* [**`createpsbt`**`  [{"txid":"hex","vout":n,"sequence":n},...] [{"address":amount},{"data":"hex"},...] ( locktime )`](createpsbt.md)
* [**`createrawtransaction`**`  [{"txid":"id","vout":n},...] [{"address":amount},{"data":"hex"},...] ( locktime )`](createrawtransaction.md)
* [**`decodepsbt`**`  "psbt"`](decodepsbt.md)
* [**`decoderawtransaction`**`  "hexstring"`](decoderawtransaction.md)
* [**`decodescript`**`  "hexstring"`](decodescript.md)
* [**`finalizepsbt`**`  "psbt" ( extract )`](finalizepsbt.md)
* [**`fundrawtransaction`**`  "hexstring" ( options )`](fundrawtransaction.md)
* [**`getrawtransaction`**`  "txid" ( verbose "blockhash" )`](getrawtransaction.md)
* [**`sendrawtransaction`**`  "hexstring" ( allowhighfees )`](sendrawtransaction.md)
* [**`signrawtransactionwithkey`**`  "hexstring" ["privatekey",...] ( [{"txid":"hex","vout":n,"scriptPubKey":"hex","redeemScript":"hex","amount":amount},...] "sighashtype" )`](signrawtransactionwithkey.md)
* [**`testmempoolaccept`**`  ["rawtxs"] ( allowhighfees )`](testmempoolaccept.md)

## Util

* [**`createmultisig`**`  nrequired ["key",...]`](createmultisig.md)
* [**`estimatefee`**](estimatefee.md)
* [**`signmessagewithprivkey`**`  "privkey" "message"`](signmessagewithprivkey.md)
* [**`validateaddress`**`  "address"`](validateaddress.md)
* [**`verifymessage`**`  "address" "signature" "message"`](verifymessage.md)

## Wallet

* [**`abandontransaction`**`  "txid"`](abandontransaction.md)
* [**`abortrescan`**](abortrescan.md)
* [**`addmultisigaddress`**`  nrequired ["key",...] ( "label" )`](addmultisigaddress.md)
* [**`backupwallet`**`  "destination"`](backupwallet.md)
* [**`createwallet`**`  "wallet_name" ( disable_private_keys )`](createwallet.md)
* [**`dumpprivkey`**`  "address"`](dumpprivkey.md)
* [**`dumpwallet`**`  "filename"`](dumpwallet.md)
* [**`encryptwallet`**`  "passphrase"`](encryptwallet.md)
* [**`getaddressesbylabel`**`  "label"`](getaddressesbylabel.md)
* [**`getaddressinfo`**`  "address"`](getaddressinfo.md)
* [**`getbalance`**`  ( "dummy" minconf include_watchonly )`](getbalance.md)
* [**`getnewaddress`**`  ( "label" )`](getnewaddress.md)
* [**`getrawchangeaddress`**](getrawchangeaddress.md)
* [**`getreceivedbyaddress`**`  "address" ( minconf )`](getreceivedbyaddress.md)
* [**`getreceivedbylabel`**`  "label" ( minconf )`](getreceivedbylabel.md)
* [**`gettransaction`**`  "txid" ( include_watchonly )`](gettransaction.md)
* [**`getunconfirmedbalance`**](getunconfirmedbalance.md)
* [**`getwalletinfo`**](getwalletinfo.md)
* [**`importaddress`**`  "address" ( "label" rescan p2sh )`](importaddress.md)
* [**`importmulti`**`  "requests" ( "options" )`](importmulti.md)
* [**`importprivkey`**`  "privkey" ( "label" ) ( rescan )`](importprivkey.md)
* [**`importprunedfunds`**`  "rawtransaction" "txoutproof"`](importprunedfunds.md)
* [**`importpubkey`**`  "pubkey" ( "label" rescan )`](importpubkey.md)
* [**`importwallet`**`  "filename"`](importwallet.md)
* [**`keypoolrefill`**`  ( newsize )`](keypoolrefill.md)
* [**`listaddressgroupings`**](listaddressgroupings.md)
* [**`listlabels`**`  ( "purpose" )`](listlabels.md)
* [**`listlockunspent`**](listlockunspent.md)
* [**`listreceivedbyaddress`**`  ( minconf include_empty include_watchonly address_filter )`](listreceivedbyaddress.md)
* [**`listreceivedbylabel`**`  ( minconf include_empty include_watchonly)`](listreceivedbylabel.md)
* [**`listsinceblock`**`  ( "blockhash" target_confirmations include_watchonly include_removed )`](listsinceblock.md)
* [**`listtransactions`**`  ( "label" count skip include_watchonly )`](listtransactions.md)
* [**`listunspent`**`  ( minconf maxconf ["address",...] include_unsafe {"minimumAmount":amount,"maximumAmount":amount,"maximumCount":n,"minimumSumAmount":amount} )`](listunspent.md)
* [**`listwalletdir`**](listwalletdir.md)
* [**`listwallets`**](listwallets.md)
* [**`loadwallet`**`  "filename"`](loadwallet.md)
* [**`lockunspent`**`  unlock ( [{"txid":"hex","vout":n},...] )`](lockunspent.md)
* [**`removeprunedfunds`**`  "txid"`](removeprunedfunds.md)
* [**`rescanblockchain`**`  ("start_height") ("stop_height")`](rescanblockchain.md)
* [**`sendmany`**`  "dummy" {"address":amount,...} ( minconf "comment" ["address",...] )`](sendmany.md)
* [**`sendtoaddress`**`  "address" amount ( "comment" "comment_to" subtractfeefromamount )`](sendtoaddress.md)
* [**`sethdseed`**`  ( "newkeypool" "seed" )`](sethdseed.md)
* [**`setlabel`**`  "address" "label"`](setlabel.md)
* [**`settxfee`**`  amount`](settxfee.md)
* [**`signmessage`**`  "address" "message"`](signmessage.md)
* [**`signrawtransactionwithwallet`**`  "hexstring" ( [{"txid":"hex","vout":n,"scriptPubKey":"hex","redeemScript":"hex","amount":amount},...] "sighashtype" )`](signrawtransactionwithwallet.md)
* [**`unloadwallet`**`  ( "wallet_name" )`](unloadwallet.md)
* [**`walletcreatefundedpsbt`**`  [{"txid":"hex","vout":n,"sequence":n},...] [{"address":amount},{"data":"hex"},...] ( locktime {"changeAddress":"str","changePosition":n,"includeWatching":bool,"lockUnspents":bool,"feeRate":n,"subtractFeeFromOutputs":[int,...]} bip32derivs )`](walletcreatefundedpsbt.md)
* [**`walletprocesspsbt`**`  "psbt" ( sign "sighashtype" bip32derivs )`](walletprocesspsbt.md)

## Zmq

* [**`getzmqnotifications`**](getzmqnotifications.md)

***

*Bitcoin Cash Node Daemon version v0.21.2*
