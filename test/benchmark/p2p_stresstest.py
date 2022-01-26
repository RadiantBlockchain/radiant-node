#!/usr/bin/env python3
# Copyright (c) 2019-2020 Jonathan Toomim
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
'''
stresstest -- test spam generation and localhost block propagation

This test will be slow at generating transactions unless
you have a very fast SSD or a ramdisk for the wallet files.
It is strongly recommended to run it on a ramdisk.
You can set one up up on Linux like this (adapt mountpoint as needed):

  sudo mount -t tmpfs size=4G /mnt/my/ramdisk
  sudo chmod a+x /mnt/my/ramdisk
  mkdir /mnt/my/ramdisk/tmp
  export TMPDIR=/mnt/my/ramdisk/tmp

Then build or copy the software you want to test, onto the ramdisk and
run this script from there.
'''

import http
import threading
import time
import os
import sys
from decimal import Decimal

sys.path.insert(0, os.path.join('..', 'functional'))
import test_framework.util # noqa: E402
from test_framework.test_framework import BitcoinTestFramework # noqa: E402
from test_framework.p2p import P2PInterface # noqa: E402


NUM_NODES = 4
# 168k tx is 32 MB
TX_PER_BLOCK = 10000
# set this below your hardware's peak generation rate if you want
# to have transaction validation happen in parallel with generation,
# or if you otherwise want to simulate lower generation rates.
MAX_GENERATION_RATE_PER_NODE = 15000

if NUM_NODES > test_framework.util.MAX_NODES:
    test_framework.util.MAX_NODES = NUM_NODES

# TestNode: A peer we use to send messages to bitcoind, and store responses.


class TestNode(P2PInterface):
    pass


class StressTest(BitcoinTestFramework):
    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = NUM_NODES
        self.extra_args = [["-blockmaxsize=32000000",
                            "-checkmempool=0",
                            "-txbroadcastrate=999999",
                            "-debugexclude=net",
                            "-debugexclude=mempool"]] * self.num_nodes

    def make_utxos(self, target=10000):
        print("Running make_utxos()...")
        rootamount = 49.0 / len(self.nodes)
        fanout = target + 1 if target < 100 else 100 if target < 100 * 50 else target // 50
        num_stages = -(-target // fanout) + 1  # rounds up
        print("Fanout={}, num_stages={}".format(fanout, num_stages))
        self.generate(self.nodes[0], 101)
        self.generate(self.nodes[0], num_stages * self.num_nodes - 1)
        time.sleep(0.2)
        self.generate(self.nodes[0], 1)
        node_addresses = [[] for _ in self.nodes]
        self.node_addresses = node_addresses
        t0 = time.time()

        def get_addresses(node, addresslist, n):
            for _ in range(n):
                addresslist.append(node.getnewaddress())
        threads = [threading.Thread(target=get_addresses,
                                    args=(self.nodes[i], node_addresses[i], fanout))
                   for i in range(len(self.nodes))]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        t1 = time.time()
        print("Generating addresses took {:3.3f} sec".format(t1 - t0))
        self.sync_blocks(timeout=10)
        for i in range(self.num_nodes - 1, 0, -1):
            amount = Decimal(round(rootamount / (fanout + 1) * 1e8)) / Decimal(1e8)
            payments = {node_addresses[i][n]: amount for n in range(fanout)}
            t1 = time.time()
            for stage in range(num_stages):
                self.nodes[0].sendmany('', payments)
            t2 = time.time()
            print("Filling node wallets took {:3.3f} sec for stage {}:{}".format(t2 - t1, i, stage))
        self.generate(self.nodes[0], 1)
        self.sync_blocks()
        for i in range(1 + (target * self.num_nodes) // 20000):
            self.generate(self.nodes[0], 1)
            self.sync_blocks(timeout=20)
            blk = self.nodes[0].getblock(self.nodes[0].getbestblockhash(), 1)
            print("Block has {} transactions and is {} bytes".format(len(blk['tx']), blk['size']))
        return amount

    def check_mempools(self):
        results = []
        for node in self.nodes:
            res = node.getmempoolinfo()
            results.append(res)
        print("Mempool sizes:\t", ("%7i " * len(self.nodes)) % tuple([r['size'] for r in results]), '\t',
              "Mempool bytes:\t", ("%9i " * len(self.nodes)) % tuple([r['bytes'] for r in results]))
        return [r['size'] for r in results]

    def generate_spam(self, value, txcount):
        def helper(node, count, rate=100):
            t = time.time()
            addresses = self.node_addresses[node]
            for i in range(0, count):
                now = time.time()
                if i / (now - t) > rate:
                    time.sleep(i / rate - (now - t))
                if not (i % 500):
                    print("Node {:2d}\ttx {:5d}\tat {:3.3f} sec\t({:3.0f} tx/sec)".format(node,
                                                                                          i, time.time() - t, (i / (time.time() - t))))
                add = addresses[i % len(addresses)]
                # We test both sendtoaddress and sendmany (both should support the coinsel as the 6th arg)
                if i % 2 == 0:
                    send_method = lambda: self.nodes[node].sendtoaddress(add, value, '', '', False, 1)
                else:
                    send_method = lambda: self.nodes[node].sendmany('', {add: value}, None, None, None, 1)
                try:
                    send_method()
                except http.client.CannotSendRequest:
                    send_method()  # Try again
                except BaseException:
                    self.log.error("Node {} had a fatal error on tx {}:".format(node, i))
                    raise
        threads = [threading.Thread(target=helper, args=(n, txcount, MAX_GENERATION_RATE_PER_NODE))
                   for n in range(1, len(self.nodes))]

        t0 = time.time()
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        t1 = time.time()
        print("Generating spam took {:3.3f} sec for {} tx (total {:4.0f} tx/sec)".format(t1 - t0,
                                                                                         (self.num_nodes - 1) * txcount, (self.num_nodes - 1) * txcount / (t1 - t0)))
        startresults = results = self.check_mempools()
        onedone = False
        finishresults = []
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
                finishresults = results
                t1b = time.time()
                onedone = True
        t2 = time.time()
        print("Mempool sync took {:3.3f} sec".format(t2 - t1))
        if timeout >= 5:
            print("Warning: Not all transactions were fully propagated")
        if not finishresults:
            t1b = time.time()
            finishresults = results
            print("Warning: Number of mempool transactions was at least 10 less than expected")
        deltas = [r - s for r, s in zip(finishresults, startresults)]
        print("Per-node ATMP tx/sec: " + ("\t%4.0f" * self.num_nodes) % tuple([d / (t1b - t1) for d in deltas]))
        print("Average mempool sync rate: \t{:4.0f} tx/sec".format(sum(deltas) / (t1b - t1) / len(deltas)))

        for i in range(2):
            t2a = time.time()
            oldheight = self.nodes[0].getblockcount()
            if not i:
                print("Generating block ", end="")
            self.generate(self.nodes[0], 1)
            t2b = time.time()
            if not i:
                print("took {:3.3f} sec".format(t2b - t2a))
            for n in range(self.num_nodes):
                while self.nodes[n].getblockcount() == oldheight:
                    time.sleep(0.05)
                t2c = time.time()
                if not i:
                    print("{}:{:6.3f}   ".format(n, t2c - t2b), end="")
            if not i:
                print()
            self.sync_blocks(timeout=180)
            t2c = time.time()
            if not i:
                print("Propagating block took {:3.3f} sec -- {:3.3f} sec per hop".format(t2c -
                                                                                         t2b, (t2c - t2b) / (self.num_nodes - 1)))
            blk = self.nodes[0].getblock(self.nodes[0].getbestblockhash(), 1)
            if not i:
                print("Block has {} transactions and is {} bytes".format(len(blk['tx']), blk['size']))

    def run_test(self):
        # Setup the p2p connections
        self.log.info("Running tests:")

        print(self.nodes[0].getmempoolinfo())

        tx_per_node = int(TX_PER_BLOCK / (self.num_nodes - 1))
        # We will need UTXOs to construct transactions in later tests.
        utxo_value = self.make_utxos(tx_per_node)
        spend_value = utxo_value

        for i in range(5):
            spend_value = Decimal((spend_value * 100000000 - 192)) / Decimal(1e8)
            print("Spam block generation round {}".format(i))
            self.generate_spam(spend_value, txcount=int(tx_per_node))


if __name__ == '__main__':
    StressTest().main()
