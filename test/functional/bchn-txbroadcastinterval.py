#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Author matricz
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test that inv messages are sent according to
an exponential distribution with scale -txbroadcastinterval
The outbound interval should be half of the inbound
"""
import time

from test_framework.cdefs import MAX_INV_BROADCAST_INTERVAL
from test_framework.p2p import P2PInterface, p2p_lock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until, connect_nodes, disconnect_nodes
from scipy import stats


class InvReceiver(P2PInterface):

    def __init__(self):
        super().__init__()
        self.invTimes = []
        self.invDelays = []

    def on_inv(self, message):

        timeArrived = time.time()
        # If an inv contains more then one transaction, then the number of invs (==samplesize)
        # will be non-deterministic. This would be an error.
        assert(len(message.inv) == 1)
        self.invTimes.append(timeArrived)
        if len(self.invTimes) > 1:
            timediff = self.invTimes[-1] - self.invTimes[-2]
            self.invDelays.append(timediff)


class TxBroadcastIntervalTest(BitcoinTestFramework):

    # This test will have a node create a number of transactions and relay them
    # to the P2P InvReceivers (one inbound and one outbound) according to test
    # parameters.
    # A third disconnected node is used only to create signed transactions

    # The nodes are configured with "-txbroadcastrate=1" and
    # "-excessiveblocksize=2000000" so that they relay at most one tx per inv
    # It's convenient, because we can now define the exact number of invs
    # (== sample size -1) that we want to send
    # This holds true only for interval values <= 500 ms

    # The P2P InvReceiver just listens and registers the delays between invs and
    # constructs a sample array from these delays
    # This sample is tested against a reference exponential distribution
    # density with the same parameters with scipy.stats.kstest
    # (See https://en.wikipedia.org/wiki/Kolmogorov%E2%80%93Smirnov_test)
    # The test is accepted if the delays sample resembles the reference
    # distribution -- or, more specifically, if the probability that the
    # observed distribution would have occurred as a sampling of the theoretical
    # exponential distribution with a probability of at least alpha
    # (pvalue > alpha, default 0.001)

    # There is one P2P interface that connects directly to the node that
    # generates transactions.
    # This tests the *inbound* connection interval.
    # The first node creates an outbound connection to the second node,
    # which relays the transactions instantly (-txbroadcastinterval=1)
    # to the second P2P interface, which tests the *outbound* connection
    # interval (= 1/2 of the inbound).
    # (but is less reliable for small values of the -txbroadcastinterval)

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def add_options(self, parser):
        parser.add_argument("--interval", dest="interval", type=int, default=500,
                            help="Set the average send interval in ms")
        parser.add_argument("--samplesize", dest="samplesize", type=int, default=100,
                            help="Set the samplesize (number of inv message delays) for testing")
        parser.add_argument("--testoutbound", dest="testoutbound", action="store_true",
                            help="Set whether to test outbound (along inbound) connection interval")
        parser.add_argument("--alpha", dest="alpha", type=float, default="0.001",
                            help="Set a confidence threshold for the kstest")

    def set_test_params(self):
        assert self.options.interval <= MAX_INV_BROADCAST_INTERVAL
        self.scale = self.options.interval / 1000
        self.num_nodes = 3
        args = [
            ["-txbroadcastinterval={}".format(self.options.interval),
                "-txbroadcastrate=1", "-excessiveblocksize=2000000",
                "-blockmaxsize=2000000"],
            ["-txbroadcastinterval=1",
                "-txbroadcastrate=1", "-excessiveblocksize=2000000",
                "-blockmaxsize=2000000"],
            []
        ]
        self.extra_args = args

    def setup_network(self):
        self.setup_nodes()
        connect_nodes(self.nodes[0], self.nodes[1])
        connect_nodes(self.nodes[1], self.nodes[2])
        # Generate enough coins on the spending nodes
        self.generate(self.nodes[2], 20 + 100)
        self.sync_all()

        # Disconnect node 3 so that it doesn't broadcast the txs it creates
        disconnect_nodes(self.nodes[1], self.nodes[2])
        self.signedtxs = []
        to = self.nodes[2].getnewaddress()
        for i in range(self.options.samplesize):
            txid = self.nodes[2].sendtoaddress(to, "0.00001", "comment", "comment_to", False, 2)
            self.signedtxs.append(self.nodes[2].gettransaction(txid)['hex'])

    def run_test(self):
        inboundReceiver, outboundReceiver = InvReceiver(), InvReceiver()
        self.nodes[0].add_p2p_connection(inboundReceiver)
        self.nodes[1].add_p2p_connection(outboundReceiver)

        for signextx in self.signedtxs:
            self.nodes[0].sendrawtransaction(signextx, True)

        wait_until(
            lambda: len(inboundReceiver.invTimes) == self.options.samplesize,
            lock=p2p_lock,
            timeout=self.options.samplesize * self.options.interval / 1000 * 2)
        wait_until(
            lambda: len(outboundReceiver.invTimes) == self.options.samplesize,
            lock=p2p_lock,
            timeout=self.options.samplesize * self.options.interval / 1000)

        inboundkstestresult = stats.kstest(inboundReceiver.invDelays, stats.expon(scale=self.scale).cdf)
        outboundkstestresult = stats.kstest(outboundReceiver.invDelays, stats.expon(scale=self.scale / 2).cdf)
        self.log.info("kstestresults for interval {}: inbound {}, outbound {}".format(
            self.options.interval,
            inboundkstestresult,
            outboundkstestresult))
        assert(inboundkstestresult.pvalue > self.options.alpha), inboundReceiver.invDelays
        if self.options.testoutbound:
            assert(outboundkstestresult.pvalue > self.options.alpha), outboundReceiver.invDelays


if __name__ == '__main__':
    TxBroadcastIntervalTest().main()
