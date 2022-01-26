#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
bchn-feature-txbroadcastinterval -- test that -txbroadcastinterval has no regressions and roughly works as expected
'''

import math
import time
from collections import defaultdict
from decimal import Decimal
from typing import Tuple, List

from test_framework.messages import ser_uint256
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until


class TXBroadcastIntervalTest(BitcoinTestFramework):

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def set_test_params(self):
        self.num_nodes = 6
        common_args = ["-spendzeroconfchange=1", "-walletbroadcast=1"]
        self.times_ms = [250, 2500, 0, 5000, 1, 500]
        self.extra_args = []
        for ms in self.times_ms:
            self.extra_args.append(['-txbroadcastinterval={}'.format(ms)])
        for args in self.extra_args:
            args.extend(common_args)

    def send_new_tx_and_wait_for_all(self, p2ps, nodenum, amount='0.00005460', timeout=60,
                                     dest_addr=None) -> Tuple[str, List[float]]:
        node = self.nodes[nodenum]
        dest_addr = node.getnewaddress() if dest_addr is None else dest_addr
        txid = node.sendtoaddress(dest_addr, Decimal(amount))
        t0 = time.time()
        self.sync_mempools(self.nodes, timeout=timeout)
        timeout -= time.time() - t0
        wait_until(
            lambda: all(txid in p2p.seen_invs for p2p in p2ps),
            timeout=timeout
        )
        normalized_time = p2ps[nodenum].seen_invs[txid]
        return txid, [max(0.0, p2p.seen_invs[txid] - normalized_time) for p2p in p2ps]

    def run_test(self):
        self.generate(self.nodes[0], 1)  # Functional test nodes believe themselves to be in IBD until you generate 1
        self.sync_all()

        class MyP2PInterface(P2PInterface):

            def __init__(self, parent, name="P2PNode"):
                super().__init__()
                self.parent = parent
                self.name = name
                # timestamps of when invs were seen. key is hex encoded inv.hash, reversed
                self.seen_invs = defaultdict(float)

            def on_inv(self, message):
                for inv in message.inv:
                    hashhex = ser_uint256(inv.hash)[::-1].hex()
                    self.seen_invs[hashhex] = time.time()
                super().on_inv(message)

        p2ps = list()
        for i, node in enumerate(self.nodes):
            p2p = MyP2PInterface(self, "P2PNode{}".format(i))
            # we connect to the node so that it applies the full txbroadcastinterval for inbound conns
            p2p.peer_connect(dstaddr='127.0.0.1', dstport=node.p2p_port, net=self.chain)()
            wait_until(
                lambda: p2p.is_connected,
                timeout=10
            )
            p2ps.append(p2p)

        fails = successes = 0
        expected = sum(t / 1e3 for t in self.times_ms)
        tolerance = 2 * math.sqrt(expected)  # allow for 2 std-deviations away from poisson mean
        for i in range(20):
            txid, times = self.send_new_tx_and_wait_for_all(p2ps, 0)
            for i in range(len(times)):
                # fudge times because node0 never applies a delay for wallet txns being sent
                times[i] += self.times_ms[0] / 1e3
            self.log.info(f"Tx {txid} times: {times}")
            end_to_end = times[-1]
            self.log.info(f"End-to-end: {end_to_end:1.4f}, expected: {expected:1.4f} +/- {tolerance:1.4f}")
            if end_to_end - tolerance < expected < end_to_end + tolerance:
                successes += 1
                self.log.info("Success!")
            else:
                fails += 1
                self.log.info("Failure :(")
            # if we got 3 in a row successes and no failures ever, assume good, bail early
            # or if our failure rate is low enough, also bail early
            if (successes >= 3 and fails == 0) or (fails > 0 and fails * 1.5 < successes):
                break
        self.log.info(f"Successes: {successes} fails: {fails}")
        assert successes > fails


if __name__ == '__main__':
    TXBroadcastIntervalTest().main()
