#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the command-line setting of the -maxmempool setting."""

from test_framework.cdefs import DEFAULT_EXCESSIVE_BLOCK_SIZE
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)


class MaxMempoolSizeTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def get_mempool_size(self):
        return self.nodes[0].getmempoolinfo()['maxmempool']

    def run_test(self):
        self.log.info('Check that -maxmempool command-line settings work')
        # Check that the -maxmempool setting defaults work correctly
        assert_equal(self.get_mempool_size(), DEFAULT_EXCESSIVE_BLOCK_SIZE * 10)

        # 2 MB excessive block size should give a 20 MB max mempool size
        self.restart_node(0, ['-blockmaxsize=2000000', '-excessiveblocksize=2000000'])
        assert_equal(self.get_mempool_size(), 2000000 * 10)

        # 256 MB excessive block size should give a 2560 MB max mempool size
        self.restart_node(0, ['-excessiveblocksize=256000000'])
        assert_equal(self.get_mempool_size(), 256000000 * 10)

        # mempool size values are floored to the next 1 MB boundary
        self.restart_node(0, ['-excessiveblocksize=255999999'])
        assert_equal(self.get_mempool_size(), 255900000 * 10)

        self.restart_node(0, ['-excessiveblocksize=256099999'])
        assert_equal(self.get_mempool_size(), 256000000 * 10)

        self.restart_node(0, ['-excessiveblocksize=256100000'])
        assert_equal(self.get_mempool_size(), 256100000 * 10)

        # excessive block size should be ignored if -maxmempool is given
        self.restart_node(0, ['-blockmaxsize=2000000', '-excessiveblocksize=2000000', '-maxmempool=300'])
        assert_equal(self.get_mempool_size(), 300 * 1000000)

        # -maxmempool setting should also be used if it is given alone
        self.restart_node(0, ['-maxmempool=123'])
        assert_equal(self.get_mempool_size(), 123 * 1000000)


if __name__ == '__main__':
    MaxMempoolSizeTest().main()
