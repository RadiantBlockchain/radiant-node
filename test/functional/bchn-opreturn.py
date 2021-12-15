#!/usr/bin/env python3
# Copyright (c) 2018-2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
This test checks activation of multiple op_return
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    ToHex,
)
from test_framework.script import CScript, OP_RETURN
from test_framework.util import satoshi_round, assert_raises_rpc_error


class OpReturnTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-whitelist=127.0.0.1',
                            '-acceptnonstdtxn=0']]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def create_null_data_tx(self, data_count):
        node = self.nodes[0]
        utxos = node.listunspent()
        assert(len(utxos) > 0)
        utxo = utxos[0]
        tx = CTransaction()
        value = int(satoshi_round((utxo["amount"] - self.relayfee) / data_count) * COIN)
        tx.vin = [CTxIn(COutPoint(int(utxo["txid"], 16), utxo["vout"]))]
        script = CScript([OP_RETURN, b'xyz'])
        tx.vout = [CTxOut(value, script)] * data_count
        tx_signed = node.signrawtransactionwithwallet(ToHex(tx))["hex"]
        return tx_signed

    def run_test(self):
        node = self.nodes[0]
        self.relayfee = self.nodes[0].getnetworkinfo()["relayfee"]

        # First, we generate some coins to spend.
        self.generate(node, 125)

        # single opreturn are ok.
        tx = self.create_null_data_tx(1)
        txid = node.sendrawtransaction(tx)
        assert(txid in set(node.getrawmempool()))

        # 2 outputs is now accepted after the May 15 2021 upgrade.
        tx = self.create_null_data_tx(2)
        txid = node.sendrawtransaction(tx)
        assert(txid in set(node.getrawmempool()))

        # 44 outputs (220 script bytes) are now accepted.
        tx = self.create_null_data_tx(44)
        txid = node.sendrawtransaction(tx)
        assert(txid in set(node.getrawmempool()))

        # 45 outputs (225 script bytes) are rejected.
        tx = self.create_null_data_tx(45)
        assert_raises_rpc_error(-26, 'oversize-op-return',
                                node.sendrawtransaction, tx)


if __name__ == '__main__':
    OpReturnTest().main()
