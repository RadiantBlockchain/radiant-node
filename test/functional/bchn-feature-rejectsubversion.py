#!/usr/bin/env python3
# Copyright (c) 2020-2021 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""" Test for the -rejectsubversion CLI arg """

from test_framework.messages import MY_SUBVERSION
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until


class RejectSubVersionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 3
        reject_subver = MY_SUBVERSION.decode('utf8').split('/')[1].split(':')[0]  # "python-p2p-tester"
        # node 0 does not reject anybody
        # node 1 does reject us based on subversion
        # node 2 has -rejectsubversion but also -whitelist, so -whitelist allows us even with a bad subversion
        self.extra_args = [
            [],
            ['-rejectsubversion=' + reject_subver],
            ['-rejectsubversion=' + reject_subver, '-whitelist=127.0.0.1/32'],
        ]

    def run_test(self):
        # create a pyton p2p node
        p2p = P2PInterface()

        # Connect it to node 0, node 0 has no -rejectsubversion
        self.nodes[0].add_p2p_connection(p2p, wait_for_verack=True)
        wait_until(
            lambda: p2p.is_connected,
            timeout=10
        )

        # Disconnect from node 0, to be polite
        self.nodes[0].disconnect_p2ps()

        class MyP2PInterface(P2PInterface):
            lost_conn = False

            def connection_lost(self, *args, **kwargs):
                """Custom handler to immediately detect dropped connections"""
                self.lost_conn = True
                return super().connection_lost(*args, **kwargs)

        # Connect to node 1, node 1 should reject us because of our subversion
        p2p = MyP2PInterface()  # create a new instance just to be sure
        p2p.peer_connect(dstaddr='127.0.0.1', dstport=self.nodes[1].p2p_port, net=self.chain)()
        wait_until(
            lambda: p2p.lost_conn,
            timeout=10
        )

        # Node 2 has -rejectsubverson the same as node 1, but it also has a -whitelist.
        # -whitelist should override -rejectsubversion
        p2p = P2PInterface()

        # Connect it to node 0, node 0 has no -rejectsubversion
        self.nodes[2].add_p2p_connection(p2p, wait_for_verack=True)
        wait_until(
            lambda: p2p.is_connected,
            timeout=10
        )

        # Disconnect from node 2, to be polite
        self.nodes[2].disconnect_p2ps()


if __name__ == '__main__':
    RejectSubVersionTest().main()
