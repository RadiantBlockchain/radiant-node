#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import os
import sys
if sys.version_info[0] < 3:
    sys.exit("Use Python 3")
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class ExtVersionDisabledTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        # Node 0 will have NODE_EXTVERSION service flag set
        # Node 1 will not have the NODE_EXTVERSION service flag set
        self.extra_args = [
            ['-useextversion=1'],
            ['-useextversion=0'],
        ]

    def run_test(self):
        self.stop_nodes()
        must_not_exist = [
            'sending extversion',
            'received: extversion',
        ]
        self.log.info("Testing that no node sent or received an extversion message")
        for node in self.nodes:
            assert node.is_node_stopped()
            debug_log = os.path.join(node.datadir, self.chain, 'debug.log')
            must_not_exist_found_ct = 0
            with open(debug_log, encoding='utf-8') as dl:
                for line in dl:
                    for substr in must_not_exist:
                        if substr in line:
                            must_not_exist_found_ct += 1
            assert_equal(must_not_exist_found_ct, 0)


if __name__ == '__main__':
    ExtVersionDisabledTest().main()
