#!/usr/bin/env python3
# Copyright (c) 2020 Calin A. Culianu <calin.culianu@gmail.com>
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""This is a BCHN-specific test which builds 2 chains on 2 disconnected
   nodes.  After the nodes are reconnected (and put on the same chain)
   they both end up with a view of 1 active and 1 inactive chain.

   This is a place to put tests that wish to test RPC calls and other
   state against inactive chains.

   For now there is 1 basic test:

     - Ensure that no regressions get introduced that take away the
       ability of `getblock` and `getblockheader` to return results for
       an inactive chain.

     - Ensure that `getblockstats` and `getchaintxstats` do not return
       results for an inactive chain."""

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    connect_nodes,
    disconnect_nodes,
)


class RPCInactiveChain(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def _disconnect_all_nodes(self):
        # Split the network
        for n1 in self.nodes:
            for n2 in self.nodes:
                if n1 is not n2:
                    disconnect_nodes(n1, n2)

    def _connect_all_nodes(self):
        # Join the network
        for n1 in self.nodes:
            for n2 in self.nodes:
                if n1 is not n2:
                    connect_nodes(n1, n2)

    def run_test(self):
        addr0 = self.nodes[0].get_deterministic_priv_key().address
        addr1 = self.nodes[1].get_deterministic_priv_key().address

        # Note: we already start with a common chain of length 200 from the framework
        self.sync_blocks()

        info0 = self.nodes[0].getblockchaininfo()
        info1 = self.nodes[1].getblockchaininfo()
        # paranoia: Check nodes are on same chain
        assert_equal(info0['bestblockhash'], info1['bestblockhash'])
        start_height = info0['blocks']

        self._disconnect_all_nodes()

        # Generate a smaller chain on node0
        shorter = self.generatetoaddress(self.nodes[0], 5, addr0)
        assert_equal(len(shorter), 5)
        # Generate a longer chain on node1
        longer = self.generatetoaddress(self.nodes[1], 10, addr1)
        assert_equal(len(longer), 10)

        info0 = self.nodes[0].getblockchaininfo()
        info1 = self.nodes[1].getblockchaininfo()
        # Ensure they are not on the same chain
        assert info0['bestblockhash'] != info1['bestblockhash']
        assert_equal(info0['bestblockhash'], shorter[-1])
        assert_equal(info1['bestblockhash'], longer[-1])
        assert_equal(info0['blocks'], start_height + len(shorter))
        assert_equal(info1['blocks'], start_height + len(longer))

        # Now, connect the nodes and sync the chains, they both will be on the longer chain
        self._connect_all_nodes()
        for node in self.nodes:
            # Due to the reorg penalty or in case shorter chain has more work,
            # we must force reorg to longer.
            node.invalidateblock(shorter[0])
        self.sync_blocks()
        info0 = self.nodes[0].getblockchaininfo()
        info1 = self.nodes[1].getblockchaininfo()
        assert_equal(info0['bestblockhash'], info1['bestblockhash'])
        assert_equal(info0['bestblockhash'], longer[-1])

        # Now, test that all of the longer chain hashes return results for the 4 RPCs we care about
        # (as a sanity check)
        for bh in longer:
            assert isinstance(self.nodes[0].getblock(bh, True), dict)
            assert isinstance(self.nodes[0].getblockheader(bh, True), dict)
            assert isinstance(self.nodes[0].getblockstats(bh, None), dict)
            assert isinstance(self.nodes[0].getchaintxstats(start_height // 2, bh), dict)

        # Now, test that all of the short inactive chain hashes:
        # 1. Return valid results for `getblock` and `getblockheader`
        # 2. Raise RPC error for `getblockstats` and `getchaintxstats`
        for bh in shorter:
            # Test that invactive chains do return valid data
            # for getblock and getblockheader
            assert isinstance(self.nodes[0].getblock(bh, True), dict)
            assert isinstance(self.nodes[0].getblockheader(bh, True), dict)
            # Just to be sure, these calls never return any data
            # for the inactive chains. Check sanity by ensuring
            # their behavior is unchanged and that "shorter" is
            # considered inactive.
            assert_raises_rpc_error(-8, f"Block is not in chain {self.chain}", self.nodes[0].getblockstats, bh, None)
            assert_raises_rpc_error(-8, "Block is not in main chain",
                                    self.nodes[0].getchaintxstats, start_height // 2, bh)


if __name__ == '__main__':
    RPCInactiveChain().main()
