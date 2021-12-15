#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test sigop and size limits for getblocktemplate."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    connect_nodes_bi
)
from test_framework.cdefs import (
    BLOCK_MAXBYTES_MAXSIGCHECKS_RATIO,
    DEFAULT_EXCESSIVE_BLOCK_SIZE,
    MAX_EXCESSIVE_BLOCK_SIZE,
    ONE_MEGABYTE
)


class GetBlockTemplateSigopsTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        self.extra_args = [[], []]

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def reinit_node(self, node_id, new_extra_args):
        self.stop_node(node_id)
        self.start_node(node_id, new_extra_args)
        connect_nodes_bi(self.nodes[0], self.nodes[1])
        self.sync_all()

    # Both getblocktemplate() and getblocktemplatelight() should yield same values for sigop and size limits
    def assert_case(self, name, node_id, excessive_size):
        self.log.info("Asserting case " + name)
        expected_sigops = excessive_size // BLOCK_MAXBYTES_MAXSIGCHECKS_RATIO

        self.log.info("- using getblocktemplate()")
        tpl = self.nodes[node_id].getblocktemplate()
        assert_equal(tpl['sigoplimit'], expected_sigops)
        assert_equal(tpl['sizelimit'], excessive_size)

        self.log.info("- using getblocktemplatelight()")
        tpl = self.nodes[node_id].getblocktemplatelight()
        assert_equal(tpl['sigoplimit'], expected_sigops)
        assert_equal(tpl['sizelimit'], excessive_size)

    def run_test(self):
        # Generate 101 blocks, setup tx and sync nodes
        self.generate(self.nodes[0], 101)
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), "1")
        self.sync_all()

        # Check against the first node, which runs with default params
        self.assert_case("when using DEFAULT_EXCESSIVE_BLOCK_SIZE", 0, DEFAULT_EXCESSIVE_BLOCK_SIZE)

        # From now on, we will test on second node with various values for excessiveblocksize

        # When below default size
        target_size = int(DEFAULT_EXCESSIVE_BLOCK_SIZE * 0.67)
        self.reinit_node(1, ["-blockmaxsize=2000000", "-excessiveblocksize=" + str(target_size)])
        self.assert_case("when below DEFAULT_EXCESSIVE_BLOCK_SIZE", 1, target_size)

        # When at lower boundary (1MB+1), but that requires blockmaxsize to be set to 1 MB as we're going below
        # the default for max generated block size (2 MB)
        target_size = ONE_MEGABYTE + 1
        self.reinit_node(1, ["-excessiveblocksize=" + str(target_size), "-blockmaxsize=" + str(ONE_MEGABYTE)])
        self.assert_case("when at lower boundary (1MB+1)", 1, target_size)

        # When slighly above lower boundary (1MB+114) but still below the default for max generated block size
        target_size = ONE_MEGABYTE + 114
        self.reinit_node(1, ["-excessiveblocksize=" + str(target_size), "-blockmaxsize=" + str(ONE_MEGABYTE)])
        self.assert_case("when slightly above the lower boundary (1MB+114)", 1, target_size)

        # When above the default max block size
        target_size = int(DEFAULT_EXCESSIVE_BLOCK_SIZE * 3.14)
        self.reinit_node(1, ["-excessiveblocksize=" + str(target_size)])
        self.assert_case("when above the DEFAULT_EXCESSIVE_BLOCK_SIZE", 1, target_size)

        # When at the upper boundary
        self.reinit_node(1, [])
        upper_boundary = MAX_EXCESSIVE_BLOCK_SIZE
        self.nodes[1].setexcessiveblock(upper_boundary)
        self.assert_case("when at the upper boundary", 1, upper_boundary)

        # When somewhere below upper boundary
        target_size = int(upper_boundary * 0.67)
        self.nodes[1].setexcessiveblock(target_size)
        self.assert_case("when somewhere below the upper boundary", 1, target_size)

        # reset to default before exit
        self.nodes[1].setexcessiveblock(DEFAULT_EXCESSIVE_BLOCK_SIZE)


if __name__ == '__main__':
    GetBlockTemplateSigopsTest().main()
