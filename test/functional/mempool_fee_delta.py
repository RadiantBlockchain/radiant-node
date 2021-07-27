#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test mempool feeDelta both positive and negative, then reorgs"""

from decimal import Decimal, getcontext

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes,
    disconnect_nodes
)


class MempoolFeeDeltaTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 4
        getcontext().prec = 8
        self.extra_args = [
            [
                '-blockmintxfee=0.000001',
            ],
            [], [], []
        ]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def create_prioritized_transaction(self, node, address, fee_delta):
        txid = node.sendtoaddress(address, 1)
        tx_initialfee = node.getmempoolentry(txid)['fees']['modified']
        node.prioritisetransaction(txid=txid, fee_delta=fee_delta)
        tx_modifiedfee = node.getmempoolentry(txid)['fees']['modified']
        return txid, tx_initialfee, tx_modifiedfee

    def run_test(self):

        address = self.nodes[0].getnewaddress()
        disconnect_nodes(self.nodes[1], self.nodes[2])

        # Create a transaction that will be never added in a block
        txid0, tx0_initialfee, tx0_modifiedfee = self.create_prioritized_transaction(
            self.nodes[0], address, -220)

        # Create 1 block for the first group of nodes to make reorg code kick in
        # below.
        self.generate(self.nodes[0], 1)
        self.sync_blocks(self.nodes[0:1])
        self.sync_mempools(self.nodes[0:1])

        # check txid0 is still in mempool
        assert self.nodes[0].getmempoolentry(txid0)

        # Create a transaction and prioritise it with negative fee delta
        txid1, tx1_initialfee, tx1_modifiedfee = self.create_prioritized_transaction(
            self.nodes[0], address, -1)
        assert_equal(tx1_modifiedfee, tx1_initialfee - Decimal(0.00000001))

        # Create a transaction and prioritise it with positive fee delta
        txid2, tx2_initialfee, tx2_modifiedfee = self.create_prioritized_transaction(
            self.nodes[0], address, 2000)
        assert_equal(tx2_modifiedfee, tx2_initialfee + Decimal(0.00002000))

        # Trigger reorg -- block created above will roll back so the DisconnectedBlockTransactions
        # code on the C++ side will kick in, temporarily emptying out the mempool and filling it
        # again. Fee deltas for in-memory txs should be preserved across this process.
        self.generate(self.nodes[3], 10)
        connect_nodes(self.nodes[1], self.nodes[2])
        self.sync_blocks()

        tx1_postreorgfee = self.nodes[0].getmempoolentry(txid1)['fees']['modified']
        tx2_postreorgfee = self.nodes[0].getmempoolentry(txid2)['fees']['modified']

        assert_equal(tx1_modifiedfee, tx1_postreorgfee)
        assert_equal(tx2_modifiedfee, tx2_postreorgfee)


if __name__ == '__main__':
    MempoolFeeDeltaTest().main()
