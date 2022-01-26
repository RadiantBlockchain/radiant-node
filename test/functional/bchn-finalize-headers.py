#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test processing of -finalizeheaders and -finalizeheaderspenalty

Setup: two nodes, node0 + node1, not connected to each other.

Node0 will be used to test behavior of receiving headers with header finalization enabled.
Node1 will test with header finalization disabled.

We have one P2PInterface connection to node0 called node_with_finalheaders,
and one to node1 called node_without_finalheaders.

We use a -finalizeheaderspenalty of 50 so that two below-finalized headers
are needed for node 0 to disconnect us.

Blocks are created so that the chains contain a finalized block.
Then, headers for blocks that would replace the finalized one, and deeper,
are created, and it is checked that the node on which headers are checked
against the finalized block, will disconnect the submitter for misbehavior.
"""

import time

from test_framework.blocktools import (
    create_block,
    create_coinbase
)
from test_framework.messages import (
    CBlockHeader,
    msg_headers,
)
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

# Just a handy constant that's often needed below.
# If the actual default value in the C++ code changes, the test would fail
# and this test constant should be adjusted.
DEFAULT_MAXREORGDEPTH = 10


def mine_header(prevblockhash, coinbase, timestamp):
    # Create a valid block and return its header
    block = create_block(int("0x" + prevblockhash, 0), coinbase, timestamp)
    block.solve()
    return CBlockHeader(block)


class FinalizedHeadersTest(BitcoinTestFramework):

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2
        # use a finalization delay of 0
        common_extra_args = ["-finalizationdelay=0", "-noparkdeepreorg"]
        self.extra_args = [common_extra_args + ["-finalizeheaders=1",
                                                "-finalizeheaderspenalty=50"],
                           common_extra_args + ["-finalizeheaders=0"]]

    def setup_network(self):
        self.setup_nodes()

    def run_test(self):
        # Setup the p2p connections
        # node_with_finalheaders connects to node0
        node_with_finalheaders = self.nodes[0].add_p2p_connection(P2PInterface())
        # node_without_finalheaders connects to node1
        node_without_finalheaders = self.nodes[1].add_p2p_connection(P2PInterface())

        genesis_hash = [n.getbestblockhash() for n in self.nodes]
        assert_equal(genesis_hash[0], genesis_hash[1])

        assert_equal(self.nodes[0].getblockcount(), 0)
        assert_equal(self.nodes[1].getblockcount(), 0)

        # Have nodes mine enough blocks to get them to finalize
        for i in range(2 * DEFAULT_MAXREORGDEPTH + 1):
            [self.generatetoaddress(n, 1, n.get_deterministic_priv_key().address)
                for n in self.nodes]
            assert_equal(self.nodes[0].getblockcount(), i + 1)
            assert_equal(self.nodes[1].getblockcount(), i + 1)

        assert_equal(self.nodes[0].getblockcount(), 2 * DEFAULT_MAXREORGDEPTH + 1)
        assert_equal(self.nodes[1].getblockcount(), 2 * DEFAULT_MAXREORGDEPTH + 1)

        # Finalized block's height is now 10

        def construct_header_for(node, height, time_stamp):
            parent_hash = node.getblockhash(height - 1)
            return mine_header(parent_hash, create_coinbase(height), time_stamp)

        # For both nodes:
        # Replacement headers for block from tip down to last
        # non-finalized block should be accepted.
        block_time = int(time.time())
        node_0_blockheight = self.nodes[0].getblockcount()
        node_1_blockheight = self.nodes[1].getblockcount()
        for i in range(1, DEFAULT_MAXREORGDEPTH):
            # Create a header for node 0 and submit it
            headers_message = msg_headers()
            headers_message.headers.append(construct_header_for(self.nodes[0],
                                                                node_0_blockheight - i,
                                                                block_time))
            node_with_finalheaders.send_and_ping(headers_message)

            # Create a header for node 1 and submit it
            headers_message = msg_headers()
            headers_message.headers.append(construct_header_for(self.nodes[1],
                                                                node_1_blockheight - i,
                                                                block_time))
            node_without_finalheaders.send_and_ping(headers_message)

            # Both nodes remain connected in this loop because
            # the new headers do not attempt to replace the finalized block
            assert node_with_finalheaders.is_connected
            assert node_without_finalheaders.is_connected

        # Now, headers that would replace the finalized block...
        # The header-finalizing node should reject the deeper header
        # and get a DoS score of 50 while the non-header-finalizing node
        # will accept the header.
        headers_message = msg_headers()
        headers_message.headers.append(construct_header_for(self.nodes[0],
                                                            node_0_blockheight - DEFAULT_MAXREORGDEPTH - 1,
                                                            block_time))
        # Node 0 has not yet been disconnected, but it got a rejection logged and penalized
        expected_header_rejection_msg = ["peer=0 (0 -> 50) reason: bad-header-finalization", ]
        with self.nodes[0].assert_debug_log(expected_msgs=expected_header_rejection_msg, timeout=10):
            node_with_finalheaders.send_and_ping(headers_message)
            # The long sleep below is for GitLab CI.
            # On local modern test machines a sleep of 1 second worked
            # very reliably.
            time.sleep(4)
        assert node_with_finalheaders.is_connected

        headers_message = msg_headers()
        headers_message.headers.append(construct_header_for(self.nodes[1],
                                                            node_0_blockheight - DEFAULT_MAXREORGDEPTH - 1,
                                                            block_time))
        node_without_finalheaders.send_message(headers_message)
        time.sleep(1)
        assert node_without_finalheaders.is_connected

        # Now, one more header on both...
        # The header-finalizing node should disconnect while the
        # non-header-finalizing node will accept the header.
        headers_message = msg_headers()
        headers_message.headers.append(construct_header_for(self.nodes[0],
                                                            node_0_blockheight - DEFAULT_MAXREORGDEPTH - 1,
                                                            block_time))
        # Node 0 should disconnect when we send again
        expected_header_rejection_msg = ["peer=0 (50 -> 100) reason: bad-header-finalization", ]
        with self.nodes[0].assert_debug_log(expected_msgs=expected_header_rejection_msg, timeout=10):
            node_with_finalheaders.send_message(headers_message)
            # Again, a long sleep below only for GitLab CI.
            time.sleep(4)
        assert not node_with_finalheaders.is_connected

        headers_message = msg_headers()
        headers_message.headers.append(construct_header_for(self.nodes[1],
                                                            node_0_blockheight - DEFAULT_MAXREORGDEPTH - 1,
                                                            block_time))
        node_without_finalheaders.send_message(headers_message)
        time.sleep(1)
        assert node_without_finalheaders.is_connected


if __name__ == '__main__':
    FinalizedHeadersTest().main()
