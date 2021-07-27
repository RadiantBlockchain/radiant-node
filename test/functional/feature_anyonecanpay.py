#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Construct a transaction using ANYONECANPAY hash type."""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, find_output


class AnyoneCanPayTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def test_anyonecanpay(self):
        # Get a mature coinbase for block 1
        self.generate(self.nodes[0], 101)
        assert_equal(Decimal('50'), self.nodes[0].getbalance())
        assert_equal(Decimal('0'), self.nodes[1].getbalance())

        # Send most of block 1 to node 1
        tx0_id = self.nodes[0].getblock(self.nodes[0].getblockhash(1))['tx'][0]
        vout0 = find_output(self.nodes[0], tx0_id, Decimal('50'))
        addr1 = self.nodes[1].getnewaddress()
        pubkey1 = self.nodes[1].getaddressinfo(addr1)['pubkey']
        inputs1 = [
            {'txid': tx0_id,
             'vout': vout0, 'amount': 49.99,
             'scriptPubKey': pubkey1}
        ]
        outputs1 = {addr1: 49.99}
        rawtx1 = self.nodes[0].createrawtransaction(inputs1, outputs1)
        # We could also use SINGLE or ALL combined with FORKID|ANYONECANPAY .
        # For no particular reason we first use NONE (no outputs covered by signature).
        signed1 = self.nodes[0].signrawtransactionwithwallet(rawtx1,
                                                             None,
                                                             "NONE|FORKID|ANYONECANPAY")
        assert 'complete' in signed1
        assert_equal(signed1['complete'], True)
        assert 'errors' not in signed1

        tx1_id = self.nodes[0].sendrawtransaction(signed1['hex'])
        self.generate(self.nodes[0], 1)
        self.sync_all()
        assert_equal(Decimal('50.00'), self.nodes[0].getbalance())
        assert_equal(Decimal('49.99'), self.nodes[1].getbalance())

        # Send back to node 0, also with SIGHASH_ANYONECANPAY.
        addr2 = self.nodes[0].getnewaddress()
        pubkey2 = self.nodes[1].getaddressinfo(addr1)['pubkey']
        vout1 = find_output(self.nodes[0], tx1_id, Decimal('49.99'))
        pubkey2 = self.nodes[0].getaddressinfo(addr2)['pubkey']
        inputs2 = [
            {'txid': tx1_id,
             'vout': vout1, 'amount': 49.98,
             'scriptPubKey': pubkey2}
        ]
        outputs2 = {addr2: 49.98}
        rawtx2 = self.nodes[1].createrawtransaction(inputs2, outputs2)
        signed2 = self.nodes[1].signrawtransactionwithwallet(rawtx2,
                                                             None,
                                                             "SINGLE|FORKID|ANYONECANPAY")
        assert 'complete' in signed2
        assert_equal(signed2['complete'], True)
        assert 'errors' not in signed2
        self.nodes[1].sendrawtransaction(signed2['hex'])
        self.generate(self.nodes[1], 1)
        self.sync_all()

        # Check balances. Node 0 got an additional full block maturing
        # on top of the 49.98 sent back to it.
        assert_equal(Decimal('149.98'), self.nodes[0].getbalance())
        # Nothing left on node 1.
        assert_equal(Decimal('0'), self.nodes[1].getbalance())

    def run_test(self):
        self.test_anyonecanpay()


if __name__ == '__main__':
    AnyoneCanPayTest().main()
