#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
bchn-rpc-gbt-checkvalidity-ignorecache
    Test that the -gbtcheckvalidity arg works, and that the template
    args "checkvalidity" and "ignorecache" work.
"""

import contextlib
import threading

from test_framework.test_framework import BitcoinTestFramework
from decimal import Decimal


class GBTCheckValidityAndIgnoreCacheTest(BitcoinTestFramework):
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 4
        # For performance, we disable the mempool check (on by default in tests)
        common_args = ['-checkmempool=0']
        self.extra_args = [common_args + ['-gbtcheckvalidity=1']] * (self.num_nodes - 1)
        # We set it so that the last node doesn't check block template validity by default
        self.extra_args.append(common_args + ['-gbtcheckvalidity=0'])

    @contextlib.contextmanager
    def assert_not_in_debug_log(self, node, excluded_msgs, timeout=2):
        """ The inverse of assert_debug_log: fail if any of the excluded_msgs are encountered in the log. """
        node_num = self.nodes.index(node)
        try:
            with node.assert_debug_log(excluded_msgs, timeout):
                yield
        except AssertionError as e:
            if 'Expected messages "{}" does not partially match log:'.format(str(excluded_msgs)) in str(e):
                return  # success
            raise
        # failure
        raise AssertionError('Some excluded messages"{}" were found in the debug log for node {}'
                             .format(str(excluded_msgs), node_num))

    def run_test(self):
        self.log.info("Generating 101 blocks ...")
        self.generate(self.nodes[-1], 101 + self.num_nodes)
        addrs = [node.getnewaddress() for node in self.nodes]
        n_txs = 32
        self.log.info("Filling mempool with {} txns ...".format(n_txs))
        fee = Decimal('0.00001000')
        amt = Decimal('50.0')
        amts = [None] * self.num_nodes
        for i in range(self.num_nodes):
            addr = addrs[i]
            amt = amt - fee
            self.log.info("Sending to node {}: {}".format(i, amt))
            self.nodes[-1].sendtoaddress(addr, amt)
            amts[i] = amt
        self.generate(self.nodes[-1], 1)
        self.sync_all()

        def thrd_func(node):
            n_tx = n_txs // len(self.nodes)
            n = self.nodes.index(node)
            node = self.nodes[n]
            addr = addrs[n]
            amt = amts[n]
            for i in range(n_tx):
                amt = amt - fee
                self.log.info("Node {}: sending {} {}/{}".format(n, amt, i + 1, n_tx))
                node.sendtoaddress(addr, amt)
            amts[n] = amt

        threads = [threading.Thread(target=thrd_func, args=(node,))
                   for node in self.nodes]
        for t in threads:
            t.start()
        for t in threads:
            t.join()

        try:
            self.sync_mempools(self.nodes, timeout=5)
        except AssertionError:
            """ We desire to synch the mempools, but it's not required for test success """

        self.log.info("Mempool sizes: {}"
                      .format([node.getmempoolinfo()["size"] for node in self.nodes]))

        """ 1. Check base functionality of the -gbtcheckvalidity option """
        block_verify_msgs = [
            '- Sanity checks:',
            '- Fork checks:',
            '- Connect ',
            '- Verify '
        ]
        # Node0 has default checkvalidity, it should have the messages associated with block verification in the debug
        with self.nodes[0].assert_debug_log(block_verify_msgs):
            self.nodes[0].getblocktemplatelight()
        # The last node has -gbtcheckvalidity=0, it should not have the messages in the debug log
        with self.assert_not_in_debug_log(self.nodes[-1], block_verify_msgs):
            self.nodes[-1].getblocktemplatelight()

        """ 2. Check that the 'ignorecache' template_request key works: """
        create_new_block_messages = [
            'CreateNewBlock():',
        ]
        # Cached, should just return same template, no CreateNewBlock() message
        with self.assert_not_in_debug_log(self.nodes[0], create_new_block_messages):
            self.nodes[0].getblocktemplatelight()
        # Ignore cache, should create new block, has CreateNewBlock() message
        with self.nodes[0].assert_debug_log(create_new_block_messages):
            self.nodes[0].getblocktemplatelight({"ignorecache": True})
        # Check one last time that the cache still is used if we specify nothing
        with self.assert_not_in_debug_log(self.nodes[0], create_new_block_messages):
            self.nodes[0].getblocktemplatelight()

        """ 3. Check that the 'checkvalidity' template_request key works on a per-call basis """
        # This node normally checks validity, we disable it, then enable it on a per-call basis
        with self.assert_not_in_debug_log(self.nodes[0], block_verify_msgs):
            self.nodes[0].getblocktemplatelight({"checkvalidity": False, "ignorecache": True})
        with self.nodes[0].assert_debug_log(block_verify_msgs):
            self.nodes[0].getblocktemplatelight({"checkvalidity": True, "ignorecache": True})
        # This node normally doesn't check validity, we enable it, then disable it on a per-call basis
        with self.nodes[-1].assert_debug_log(block_verify_msgs):
            self.nodes[-1].getblocktemplatelight({"checkvalidity": True, "ignorecache": True})
        with self.assert_not_in_debug_log(self.nodes[-1], block_verify_msgs):
            self.nodes[-1].getblocktemplatelight({"checkvalidity": False, "ignorecache": True})


if __name__ == '__main__':
    GBTCheckValidityAndIgnoreCacheTest().main()
