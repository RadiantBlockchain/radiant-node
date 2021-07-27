#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Additional tests of ancestor/descendant limit & counting scenarios.
Refers to BCHN GitLab https://gitlab.com/bitcoin-cash-node/bitcoin-cash-node/-/issues/225
and scenarios raised for investigation by TG @readdotcash in
https://bitcoincashresearch.org/t/specific-needs-for-increasing-or-removing-chained-tx-limit/240"""

from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    satoshi_round,
)


class MempoolPackagesAdditionalTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 2
        common_params = ["-maxorphantx=1000"]
        self.extra_args = [common_params,
                           common_params]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    # Build a transaction that spends parent_txid:vout
    # Return amount sent
    def chain_transaction(self, node, parent_txid, vout,
                          value, fee, num_outputs):
        send_value = satoshi_round((value - fee) / num_outputs)
        inputs = [{'txid': parent_txid, 'vout': vout}]
        outputs = {}
        for i in range(num_outputs):
            outputs[node.getnewaddress()] = send_value
        rawtx = node.createrawtransaction(inputs, outputs)
        signedtx = node.signrawtransactionwithwallet(rawtx)
        txid = node.sendrawtransaction(signedtx['hex'])
        fulltx = node.getrawtransaction(txid, 1)
        # make sure we didn't generate a change output
        assert len(fulltx['vout']) == num_outputs
        return (txid, send_value)

    def run_test(self):
        # Mine some blocks and have them mature.
        self.generate(self.nodes[0], 101)
        fee = Decimal("0.0001")

        # begin issue 225 tests
        utxo = self.nodes[0].listunspent(9)
        txid = utxo[0]['txid']
        value = utxo[0]['amount']

        # create a tx with 50 outputs
        (txid, sent_value) = self.chain_transaction(
            self.nodes[0], txid, 0, value, fee, 50)
        self.log.info("fat utxo:" + txid)

        # Check mempool, getmempoolentry / getrawmempool
        mempool = self.nodes[0].getrawmempool(True)
        assert_equal(len(mempool), 1)
        entry = self.nodes[0].getmempoolentry(txid)
        assert_equal(entry, mempool[txid])

        # Build 49 chained 1-input-one-output txs on top of the 50-output tx
        # Pre-tachyon: If we used all 50, we would exceed descendant limit in this loop!
        # Now: We may loop as much as we like until funds are exhausted (no ancestor limit).
        fanout_txid = [''] * 50
        fanout_sent_values = [0, ] * 50
        for i in range(49):
            (fanout_txid[i], fanout_sent_values[i]) = self.chain_transaction(
                self.nodes[0], txid, i, sent_value, fee, 1)

        # check mempool again
        mempool = self.nodes[0].getrawmempool(True)
        assert_equal(len(mempool), 50)
        entry = self.nodes[0].getmempoolentry(txid)
        assert_equal(entry, mempool[txid])
        for i in range(49):
            entry = self.nodes[0].getmempoolentry(fanout_txid[i])
            assert_equal(entry, mempool[fanout_txid[i]])

        # Adding one more transaction (using the 50th vout of the fat tx) used to fail due to descendant limit hit.
        # Now, it always succeeds.
        self.chain_transaction(self.nodes[0], txid, 49, sent_value, fee, 1)

        # Adding a descendent to one of the slim txs used to also fail.
        # Now, it always succeeds.
        self.chain_transaction(self.nodes[0], fanout_txid[0], 0, fanout_sent_values[0], fee, 1)

        # clear mempool
        self.generate(self.nodes[0], 1)
        # TODO: more tests

        # end issue 225 tests


if __name__ == '__main__':
    MempoolPackagesAdditionalTest().main()
