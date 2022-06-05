#!/usr/bin/env python3
# Copyright (c) 2019-2020 Jonathan Toomim
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
radn-rpc-getblocktemplate-timing -- test that -maxgbttime and -maxinitialgbttime command-line options work
'''

import http
import threading
import time
import traceback

from test_framework.test_framework import BitcoinTestFramework
from test_framework.authproxy import JSONRPCException
from decimal import Decimal


NUM_NODES = 4
TX_PER_BLOCK = 2000


class GBTTimingTest(BitcoinTestFramework):
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = NUM_NODES
        self.extra_args = [
            # node 0: makes nearly-empty blocks.
            # node 0 also is used to ensure that maxinitialgbttime is ignored if maxinitial is greater than maxgbttime
            ['-checkmempool=0', '-maxgbttime=1', '-maxinitialgbttime=99999'],
            # node 1 checks that initial works when initial is less than maxgbttime
            ['-checkmempool=0', '-maxgbttime=15', '-maxinitialgbttime=5'],
            # node 2 checks that initial is used when initial maxgbttime is 0
            ['-checkmempool=0', '-maxgbttime=0', '-maxinitialgbttime=5'],
            # node 3 checks that initial is ignored if set to 0
            # node 3 also checks that full blocks can be produced if maxgbttime is big
            ['-checkmempool=0', '-maxgbttime=99999', '-maxinitialgbttime=0']]

    def run_test(self):
        self.setup_test()

        passes = fails = 0
        success_threshold = 0.50
        max_tries = 13

        while (passes <= (passes + fails) * success_threshold) and (passes + fails < max_tries):
            try:
                self.test_run()
                passes += 1
                self.log.info("Test run {} passed. {} passes so far.".format(fails + passes, passes))
            except AssertionError:
                fails += 1
                self.log.info("Test run {} failed. {} fails so far.".format(fails + passes, fails))
                self.log.info(traceback.format_exc())
                self.log.info("")

        self.log.info("{} passes, {} fails. Pass rate: {:2.0f}%".format(
            passes, fails, 100 * (passes / (passes + fails))))
        assert(passes > (passes + fails) * success_threshold)

    def test_run(self):
        # Calibrate performance constants: node 3 will make blocks that are limited only by available tx count
        t1 = self.time_generation(3)
        junk, n1 = self.time_generation(3, 'gbt')
        tx_sec = n1 / t1
        # -50%, +100% acceptable error margin
        max_gbt_tx_1_ms = int(tx_sec * 0.001 * 2)
        max_gbt_tx_5_ms = int(tx_sec * 0.005 * 2)
        min_gbt_tx_5_ms = int(tx_sec * 0.005 / 2)

        rpc_overhead = self.time_generation(3) * 2
        generate_overhead = self.time_generation(0, 'generate')[0] * 2

        self.log.info("  max_gbt_tx_1_ms:   {} tx".format(max_gbt_tx_1_ms))
        self.log.info("  max_gbt_tx_5_ms:   {} tx".format(max_gbt_tx_5_ms))
        self.log.info("  min_gbt_tx_5_ms:   {} tx".format(min_gbt_tx_5_ms))
        self.log.info("  rpc_overhead:      {:4.1f} ms".format(rpc_overhead * 1000))
        self.log.info("  generate_overhead: {:4.1f} ms".format(generate_overhead * 1000))

        # Let the nodes calibrate for this transaction mix over 5 rounds
        for i in range(5):
            for node in self.nodes:
                self.time_generation(node, 'gbtl')
            self.time_generation(0, 'generate')

        # First set of tests are for timing. In these tests, we use getblocktemplatelight to avoid RPC and json overhead
        # node 0: maxinitialgbttime (1000) is ignored if maxinitial is greater than maxgbttime (1)
        # First call should be fast (1 ms), second call should be cached
        t1 = self.time_generation(0)
        assert t1 < 1.5 * 0.001 + rpc_overhead

        # node 1: first call should be fast (5 ms), second call should be slower (15 ms), third should be cached/instant
        t1 = self.time_generation(1)
        t2 = self.time_generation(1)
        t3 = self.time_generation(1)
        assert t1 < 1.5 * 0.005 + rpc_overhead
        assert t2 < 1.5 * 0.015 + rpc_overhead and t2 > 0.5 * 0.015
        assert t3 < rpc_overhead
        assert t1 < t2
        assert t3 < t2

        # node 2: first call should be medium (10 ms), second call should be slow (unlimited), third call cached
        t1 = self.time_generation(2)
        t2 = self.time_generation(2)
        t3 = self.time_generation(2)
        assert t1 < 1.5 * 0.005 + rpc_overhead and t1 > 0.5 * 0.005
        assert t2 > 1.5 * 0.005 + rpc_overhead
        assert t3 < rpc_overhead
        assert t1 < t2
        assert t3 < t2

        # node 3: First call should be big, second call should be cached
        # Because the cached template is big, give it a bit longer
        t1 = self.time_generation(3)
        t2 = self.time_generation(3)
        assert t1 > 1.5 * 0.005 + rpc_overhead
        assert t2 < rpc_overhead
        assert t1 > t2

        # node 0: generate should be fast (1 ms)
        t, n = self.time_generation(0, 'generate')
        assert t < 1.5 * 0.001 + generate_overhead
        # End of timing tests

        # node 0: generate should be fast (1 ms)
        t, n = self.time_generation(0, 'generate')
        assert t < 1.5 * 0.001 + generate_overhead

    def time_generation(self, node, mode='gbtl', rewind=True):
        start = time.time()
        if isinstance(node, int):
            node = self.nodes[node]
        if mode == 'gbt':
            template = node.getblocktemplate()
            return time.time() - start, len(template['transactions'])
        elif mode == 'gbtl':
            node.getblocktemplatelight()
            return time.time() - start
        else:
            blockhash = self.generate(node, 1)[0]
            num_tx = node.getblock(blockhash)['nTx']
            end = time.time()
            if rewind:
                # make sure all nodes change their pindexPrev value in getblocktemplatecommon so that the next call
                # isn't cached
                self.sync_blocks()
                for n in self.nodes:
                    n.getblocktemplatelight()
                    n.invalidateblock(blockhash)
                self.sync_blocks()
            return end - start, num_tx

    def setup_test(self):
        # Setup the p2p connections
        self.log.info("Running tests:")

        tx_per_node = int(TX_PER_BLOCK / (self.num_nodes - 1))
        # We will need UTXOs to construct transactions in later tests.
        utxo_value = self.make_utxos(tx_per_node)
        spend_value = utxo_value
        spend_value = Decimal((spend_value * 100000000 - 500)) / Decimal(1e8)
        self.generate_spam(spend_value, txcount=int(tx_per_node))

    def make_utxos(self, target=10000):
        rootamount = 49.0 / len(self.nodes)
        fanout = target + 1 if target < 100 else 100 if target < 100 * 50 else target // 50
        num_stages = -(-target // fanout) + 1  # rounds up
        self.generate(self.nodes[3], 101)
        self.generate(self.nodes[3], num_stages * self.num_nodes - 1)
        time.sleep(0.2)
        self.generate(self.nodes[3], 1)
        node_addresses = [[] for _ in self.nodes]
        self.node_addresses = node_addresses

        def get_addresses(node, addresslist, n):
            for _ in range(n):
                addresslist.append(node.getnewaddress())
        threads = [threading.Thread(target=get_addresses,
                                    args=(self.nodes[i], node_addresses[i], fanout))
                   for i in range(len(self.nodes) - 1)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.sync_blocks(timeout=10)
        for i in range(self.num_nodes - 2, -1, -1):
            amount = Decimal(round(rootamount / (fanout + 1) * 1e8)) / Decimal(1e8)
            payments = {node_addresses[i][n]: amount for n in range(fanout)}
            for stage in range(num_stages):
                self.nodes[3].sendmany('', payments)
        self.generate(self.nodes[3], 1)
        self.sync_blocks()
        for i in range(1 + (target * self.num_nodes) // 20000):
            self.generate(self.nodes[3], 1)
            self.sync_blocks(timeout=20)
            self.nodes[3].getblock(self.nodes[3].getbestblockhash(), 1)
        return amount

    def check_mempools(self):
        results = []
        for node in self.nodes:
            res = node.getmempoolinfo()
            results.append(res)
        return [r['size'] for r in results]

    def generate_spam(self, value, txcount):
        def helper(node, count, rate=100):
            t = time.time()
            addresses = self.node_addresses[node]
            for i in range(0, count):
                now = time.time()
                # sleeping for 5 ms per tx ensures that bitcoind doesn't get starved for cs_main time and allows
                # transactions time to propagate to other nodes
                time.sleep(0.005)
                if i / (now - t) > rate:
                    time.sleep(i / rate - (now - t))
                add = addresses[i % len(addresses)]
                try:
                    self.nodes[node].sendtoaddress(add, value, '', '', False, 2)
                except http.client.CannotSendRequest:
                    self.nodes[node].sendtoaddress(add, value, '', '', False, 2)
                except JSONRPCException:
                    print("Warning: this bitcoind appears to not support the 'fast' argument for sendtoaddress")
                    self.nodes[node].sendtoaddress(add, value, '', '', False)

        threads = [threading.Thread(target=helper, args=(n, txcount))
                   for n in range(len(self.nodes) - 1)]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        results = self.check_mempools()
        onedone = False
        timeout = 0
        oldresults = self.check_mempools()
        while [r for r in results if abs(r - results[0]) > 10] and (timeout < 5):
            time.sleep(1)
            results = self.check_mempools()
            if results == oldresults:
                timeout += 1
            else:
                timeout = 0
            oldresults = results
            if not onedone and [r for r in results if abs(r - txcount * (self.num_nodes - 1)) < 10]:
                onedone = True


if __name__ == '__main__':
    GBTTimingTest().main()
