#!/usr/bin/env python3
# Copyright (c) 2022 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the tor-specific p2p port"""

from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_not_equal, MAX_NODES, p2p_port


class OnionPortTest(BitcoinTestFramework):
    """Test the tor-specific p2p port, ensuring that the -bind=<HOST>:<PORT>=onion syntax works"""

    def set_test_params(self):
        self.setup_clean_chain = False
        self.num_nodes = 1

    def setup_network(self):
        self.setup_nodes()

    def setup_nodes(self):
        self.onion_port = p2p_port(MAX_NODES-1)
        self.log.info(f"Starting node 0 with an extra onion port {self.onion_port}")
        # Create an additional port aside from the normal bind p2p port, for onion incoming connections.
        # We leverage the p2p_port() function to get a unique port guaranteed to be available.
        assert_not_equal(self.onion_port, p2p_port(0))
        self.add_nodes(self.num_nodes, [["-listenonion=1", f"-bind=127.0.0.1:{self.onion_port}=onion"]])
        self.start_nodes()

    def run_test(self):
        self.log.info('Creating a p2p connection for the regular port and the onion port')
        node = self.nodes[0]
        ports = (node.p2p_port, self.onion_port)
        assert_not_equal(ports[0], ports[1])
        for port in ports:
            p2p = P2PInterface()
            self.log.info(f"Ensuring that expected port {port} is reachable via p2p")
            node.add_p2p_connection(p2p, dstport=port, wait_for_verack=True)
            peers = node.getpeerinfo()
            assert_equal(len(peers), 1)
            bindport = int(peers[0]["addrbind"].split(':')[-1])
            assert_equal(bindport, port)
            if port == self.onion_port:
                # Ensure onion port has PF_NONE
                assert_equal(len(peers[0]["permissions"]), 0)
            p2p.peer_disconnect()
            assert p2p is node.p2ps[-1]
            del node.p2ps[-1]  # just ensure it's gone from the internal list


if __name__ == '__main__':
    OnionPortTest().main()
