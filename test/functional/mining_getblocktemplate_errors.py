#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test error modes for getblocktemplate."""

import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    connect_nodes_bi,
    disconnect_nodes,
    assert_raises_rpc_error,
    assert_blocktemplate_equal
)


class GetBlockTemplateTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    # This test shouldn't strictly require a wallet, but it makes things more convenient

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def template_contains_tx(self, template, txid):
        for tx in template['transactions']:
            if tx['txid'] == txid:
                return True
        return False

    def run_test(self):

        # When bitcoind is just started it's in IBD
        assert_raises_rpc_error(-10, "Bitcoin is downloading blocks...", self.nodes[0].getblocktemplate)

        # Mature some coins for easy spending, have a tx in the mempool
        self.generate(self.nodes[0], 101)
        txid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), "1")
        self.sync_all()

        # The nodes are connected, happy case
        gbt = self.nodes[0].getblocktemplate()
        assert self.template_contains_tx(gbt, txid)

        # Disconnect the nodes and verify that getblocktemplate fails
        # This is failfast behaviour, miners don't want to waste cycles
        # when they don't know if they're on the latest tip
        # and/or can't propagate blocks
        disconnect_nodes(self.nodes[0], self.nodes[1])
        assert_raises_rpc_error(-9, "Bitcoin is not connected!", self.nodes[0].getblocktemplate)
        assert_raises_rpc_error(-9, "Bitcoin is not connected!", self.nodes[1].getblocktemplate)

        # Reconnect the nodes and check that getblocktemplate works again
        # and that they're in sync
        connect_nodes_bi(self.nodes[0], self.nodes[1])
        gbt0 = self.nodes[0].getblocktemplate()
        gbt1 = self.nodes[1].getblocktemplate()
        assert_blocktemplate_equal(gbt0, gbt1)

        # Test that getblocktemplate will return a cached template
        # for the next 5 seconds
        mock_time = int(time.time())
        self.nodes[0].setmocktime(mock_time)
        template = self.nodes[0].getblocktemplate()

        # Add a new tx to the mempool
        newtxid = self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), "1")

        # Fastforward 4 seconds, the template has not changed
        self.nodes[0].setmocktime(mock_time + 4)
        new_template = self.nodes[0].getblocktemplate()
        assert new_template == template
        assert not self.template_contains_tx(new_template, newtxid)

        # 5 seconds is when the cache expires, so it is a boundary condition
        # that would introduce non-determinism to the test. We test 6 seconds, instead.
        # Fastforward 6 seconds, the new tx has now been included in the template
        self.nodes[0].setmocktime(mock_time + 6)
        new_template = self.nodes[0].getblocktemplate()
        assert new_template != template
        assert self.template_contains_tx(new_template, newtxid)


if __name__ == '__main__':
    GetBlockTemplateTest().main()
