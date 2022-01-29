#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import os
import re
import sys
if sys.version_info[0] < 3:
    sys.exit("Use Python 3")
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_greater_than


class ExtVersionNoMisbehavingTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        args = [
            '-useextversion=1',
        ]
        self.extra_args = [args] * 2

    def run_test(self):
        self.stop_nodes()
        must_exist_after = [
            (re.compile(r'sending verack'), re.compile(r'sending getaddr')),
        ]
        must_not_exist = [
            re.compile(r'Misbehaving: .* [(]0 -> 10[)] reason: missing-verack'),
            re.compile(r'Unsupported message "[^"]*" prior to verack'),
        ]
        for node in self.nodes:
            assert node.is_node_stopped()
            debug_log = os.path.join(node.datadir, self.chain, 'debug.log')
            must_exist_found_first_ct = 0
            must_exist_after_ok_ct = 0
            must_not_exist_found_ct = 0
            with open(debug_log, encoding='utf-8') as dl:
                for line in dl:
                    for rx_first, rx_second in must_exist_after:
                        if not must_exist_found_first_ct:
                            if rx_first.search(line):
                                must_exist_found_first_ct += 1
                                continue
                        if rx_second.search(line):
                            if must_exist_found_first_ct:
                                must_exist_after_ok_ct += 1
                            else:
                                raise RuntimeError("Sent the VERACK after the GETADDR message")
                    for rx in must_not_exist:
                        if rx.search(line):
                            must_not_exist_found_ct += 1
            self.log.info("Testing that the node did not penalize the other node for misbehaving")
            assert_equal(must_not_exist_found_ct, 0)
            self.log.info("Testing the that node sent its getaddr message after its verack message")
            assert_greater_than(must_exist_after_ok_ct, 0)
            self.log.info("Testing the that node sent its getaddr precisely once")
            assert_equal(must_exist_after_ok_ct, 1)


if __name__ == '__main__':
    ExtVersionNoMisbehavingTest().main()
