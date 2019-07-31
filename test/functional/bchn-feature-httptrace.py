#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test BCHN httptrace logging category."""

import os
import mmap

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal


class FeatureHttpTraceTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    @staticmethod
    def find_in_file(filename: str, string: bytes) -> bool:
        with open(filename, 'rb', 0) as fd:
            mapped = mmap.mmap(fd.fileno(), 0, access=mmap.ACCESS_READ)
            return mapped.find(string) != -1

    def run_test(self):
        debug_log_path = os.path.join(self.nodes[0].datadir, self.chain, "debug.log")

        log_marker = ', "method": "logging", "params": {},'.encode()

        def purge_log():
            os.unlink(debug_log_path)

        logging_enabled = self.nodes[0].logging()
        # Check that httptrace is present
        assert_equal('httptrace' in logging_enabled, True)
        # We started with default options so httptrace should be disabled
        assert_equal(logging_enabled['httptrace'], False)
        assert_equal(self.find_in_file(debug_log_path, log_marker), False)

        self.restart_node(0, ["-debug=all"], before_start=purge_log)
        logging_enabled = self.nodes[0].logging()
        # We started with -debug=all, so httptrace should be disabled
        assert_equal(logging_enabled['httptrace'], False)
        assert_equal(self.find_in_file(debug_log_path, log_marker), False)

        self.restart_node(
            0, ["-debug=all", "-debug=httptrace"], before_start=purge_log)
        logging_enabled = self.nodes[0].logging()
        # We explicitly enabled httptrace, so it should be enabled
        assert_equal(logging_enabled['httptrace'], True)
        assert_equal(self.find_in_file(debug_log_path, log_marker), True)

        self.restart_node(0, ["-debug=httptrace"], before_start=purge_log)
        logging_enabled = self.nodes[0].logging()
        # We explicitly enabled httptrace, so it should be enabled
        assert_equal(logging_enabled['httptrace'], True)
        assert_equal(self.find_in_file(debug_log_path, log_marker), True)


if __name__ == '__main__':
    FeatureHttpTraceTest().main()
