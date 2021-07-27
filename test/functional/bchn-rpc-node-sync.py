#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test sync_all."""

from test_framework.test_framework import BitcoinTestFramework


class NodeSetupAndSyncTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def run_test(self):
        assert self.nodes[0].getmempoolinfo()["loaded"]
        assert self.nodes[1].getmempoolinfo()["loaded"]

        # Mature some coins for easy spending, have a tx in the mempool
        self.generate(self.nodes[0], 101)
        self.sync_all()  # to prevent bad-txns-nonfinal on the following sendtoaddress
        self.nodes[0].sendtoaddress(self.nodes[1].getnewaddress(), "1")
        self.sync_all()


if __name__ == '__main__':
    NodeSetupAndSyncTest().main()
