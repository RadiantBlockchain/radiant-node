#!/usr/bin/env python3
# Copyright (c) 2018-2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
This test checks activation of multiple op_return
"""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.blocktools import create_block, create_coinbase
from test_framework.messages import (
    COIN,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    ToHex,
)
from test_framework.script import CScript, OP_RETURN
from test_framework.util import satoshi_round, assert_equal, assert_raises_rpc_error

# far into the future
TACHYON_START_TIME = 2000000000


class OpReturnActivationTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [['-whitelist=127.0.0.1',
                            '-acceptnonstdtxn=0',
                            '-tachyonactivationtime={}'.format(TACHYON_START_TIME)]]

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
        node.generate(125)

        # Check that multiple opreturn are not accepted yet.
        self.log.info("Running null-data test, before multiple data activation")

        # single opreturn are ok.
        tx = self.create_null_data_tx(1)
        txid = node.sendrawtransaction(tx)
        assert(txid in set(node.getrawmempool()))

        # multiple opreturn is non standard
        tx = self.create_null_data_tx(2)
        assert_raises_rpc_error(-26, 'multi-op-return',
                                node.sendrawtransaction, tx)

        # Push MTP forward just before activation.
        self.log.info("Pushing MTP just before the activation")
        node.setmocktime(TACHYON_START_TIME)

        def next_block(block_time):
            # get block height
            blockchaininfo = node.getblockchaininfo()
            height = int(blockchaininfo['blocks'])

            # create the block
            coinbase = create_coinbase(height)
            coinbase.rehash()
            block = create_block(
                int(node.getbestblockhash(), 16), coinbase, block_time)

            # Do PoW, which is cheap on regnet
            block.solve()
            node.submitblock(ToHex(block))

        for i in range(6):
            next_block(TACHYON_START_TIME + i - 1)

        # Check we are just before the activation time
        assert_equal(node.getblockheader(node.getbestblockhash())['mediantime'],
                     TACHYON_START_TIME - 1)

        # Check that multiple opreturn are not accepted yet.
        self.log.info("Re-running null-data test just before activation")

        # single opreturn are ok.
        tx = self.create_null_data_tx(1)
        txid = node.sendrawtransaction(tx)
        assert(txid in set(node.getrawmempool()))

        # multiple opreturn is non standard
        tx = self.create_null_data_tx(2)
        assert_raises_rpc_error(-26, 'multi-op-return',
                                node.sendrawtransaction, tx)

        # Activate multiple opreturn.
        self.log.info("Running null-data test, after multiple data activation")
        next_block(TACHYON_START_TIME + 6)

        assert_equal(node.getblockheader(node.getbestblockhash())['mediantime'],
                     TACHYON_START_TIME)

        # 2 outputs is now accepted.
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

        # Because these transactions are valid regardless, there is
        # no point checking for reorg. Worst case scenario if a reorg
        # happens, we have a few transactions in the mempool that won't
        # propagate to nodes that aren't aware of them already.


if __name__ == '__main__':
    OpReturnActivationTest().main()
