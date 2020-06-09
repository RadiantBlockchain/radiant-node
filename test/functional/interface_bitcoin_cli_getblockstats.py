#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test bitcoin-cli getblockstats"""
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)


class TestBitcoinCliGetBlockStats(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        """Main test logic"""

        # get chain's genesis block hash, we will be using it a few times
        block_0_hash = self.nodes[0].getblockhash(0)

        # check that call using simple block height works naturally
        self.log.info("Testing `bitcoin-cli getblockstats 0`")
        cli_response = self.nodes[0].cli("getblockstats", 0).send_cli()
        assert_equal(cli_response['blockhash'], block_0_hash)

        # check that it works with quoted strings
        self.log.info("Testing `bitcoin-cli getblockstats '\"{}\"'`".format(block_0_hash))
        cli_response = self.nodes[0].cli("getblockstats", "\"{}\"".format(block_0_hash)).send_cli()
        assert_equal(cli_response['blockhash'], block_0_hash)

        # check that it works without the quotes
        self.log.info("Testing `bitcoin-cli getblockstats {}`".format(block_0_hash))
        cli_response = self.nodes[0].cli("getblockstats", "{}".format(block_0_hash)).send_cli()
        assert_equal(cli_response['blockhash'], block_0_hash)


if __name__ == '__main__':
    TestBitcoinCliGetBlockStats().main()
