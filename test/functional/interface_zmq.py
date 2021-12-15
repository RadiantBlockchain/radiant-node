#!/usr/bin/env python3
# Copyright (c) 2015-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the ZMQ notification interface."""
import struct
from io import BytesIO

from test_framework.blocktools import create_raw_transaction
from test_framework.test_framework import BitcoinTestFramework
from test_framework.messages import CTransaction
from test_framework.util import (
    assert_equal,
    assert_raises_rpc_error,
    hash256_reversed,
)


ADDRESS = "tcp://127.0.0.1:28332"


class ZMQSubscriber:
    def __init__(self, socket, topic):
        self.sequence = 0
        self.socket = socket
        self.topic = topic

        import zmq
        self.socket.setsockopt(zmq.SUBSCRIBE, self.topic)

    def receive(self):
        topic, body, seq = self.socket.recv_multipart()
        # Topic should match the subscriber topic.
        assert_equal(topic, self.topic)
        # Sequence should be incremental.
        assert_equal(struct.unpack('<I', seq)[-1], self.sequence)
        self.sequence += 1
        return body


class ZMQTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_py3_zmq()
        self.skip_if_no_bitcoind_zmq()
        self.skip_if_no_wallet()

    def setup_nodes(self):
        import zmq

        # Initialize ZMQ context and socket.
        # All messages are received in the same socket which means that this
        # test fails if the publishing order changes.
        # Note that the publishing order is not defined in the documentation and
        # is subject to change.
        self.zmq_context = zmq.Context()
        socket = self.zmq_context.socket(zmq.SUB)
        socket.set(zmq.RCVTIMEO, 60000)
        socket.connect(ADDRESS)

        # Subscribe to all available topics.
        self.hashblock = ZMQSubscriber(socket, b"hashblock")
        self.hashtx = ZMQSubscriber(socket, b"hashtx")
        self.rawblock = ZMQSubscriber(socket, b"rawblock")
        self.rawtx = ZMQSubscriber(socket, b"rawtx")
        self.hashds = ZMQSubscriber(socket, b"hashds")
        self.rawds = ZMQSubscriber(socket, b"rawds")

        self.extra_args = [
            ["-zmqpub{}={}".format(sub.topic.decode(), ADDRESS) for sub in [
                self.hashblock, self.hashtx, self.rawblock, self.rawtx, self.hashds, self.rawds]],
            [],
        ]
        self.add_nodes(self.num_nodes, self.extra_args)
        self.start_nodes()

    def run_test(self):
        try:
            self._zmq_test()
        finally:
            # Destroy the ZMQ context.
            self.log.debug("Destroying ZMQ context")
            self.zmq_context.destroy(linger=None)

    def _zmq_test(self):
        num_blocks = 5
        self.log.info(
            "Generate {0} blocks (and {0} coinbase txes)".format(num_blocks))
        genhashes = self.generate(self.nodes[0], num_blocks)
        self.sync_all()

        for x in range(num_blocks):
            # Should receive the coinbase txid.
            txid = self.hashtx.receive()

            # Should receive the coinbase raw transaction.
            hex = self.rawtx.receive()
            tx = CTransaction()
            tx.deserialize(BytesIO(hex))
            tx.calc_sha256()
            assert_equal(tx.hash, txid.hex())

            # Should receive the generated block hash.
            hash = self.hashblock.receive().hex()
            assert_equal(genhashes[x], hash)
            # The block should only have the coinbase txid.
            assert_equal([txid.hex()], self.nodes[1].getblock(hash)["tx"])

            # Should receive the generated raw block.
            block = self.rawblock.receive()
            assert_equal(genhashes[x], hash256_reversed(block[:80]).hex())

        self.log.info("Wait for tx from second node")
        payment_txid = self.nodes[1].sendtoaddress(
            self.nodes[0].getnewaddress(), 1.0)
        self.sync_all()

        # Should receive the broadcasted txid.
        txid = self.hashtx.receive()
        assert_equal(payment_txid, txid.hex())

        # Should receive the broadcasted raw transaction.
        hex = self.rawtx.receive()
        assert_equal(payment_txid, hash256_reversed(hex).hex())

        self.log.info("Test the getzmqnotifications RPC")
        assert_equal(self.nodes[0].getzmqnotifications(), [
            {"type": "pubhashblock", "address": ADDRESS},
            {"type": "pubhashds", "address": ADDRESS},
            {"type": "pubhashtx", "address": ADDRESS},
            {"type": "pubrawblock", "address": ADDRESS},
            {"type": "pubrawds", "address": ADDRESS},
            {"type": "pubrawtx", "address": ADDRESS},
        ])

        assert_equal(self.nodes[1].getzmqnotifications(), [])

        # Do a double-spend and verify that we got the double-spend tx notifications (hashds & rawds)
        self.log.info("Creating double-spend transactions")
        fee = 1000 / 1e8
        amt, vout = None, None
        tx = CTransaction()
        tx.deserialize(BytesIO(hex))
        # Find the vout that we are able to sign from the previous payment tx
        for i, txout in enumerate(tx.vout):
            if txout.nValue == int(1.0 * 1e8):
                vout = i
                amt = txout.nValue
        assert amt is not None
        amt /= 1e8
        assert amt > fee * 2
        self.log.info(f"Spending {amt} from {payment_txid}:{vout}, fee: {fee}")
        ds_txs = [None, None]
        addr = self.nodes[0].getnewaddress()
        ds_txs[0] = create_raw_transaction(self.nodes[0], payment_txid, addr, amt - fee, vout)
        self.log.info("Signed tx 0")
        ds_txs[1] = create_raw_transaction(self.nodes[0], payment_txid, addr, amt - fee * 2, vout)
        self.log.info("Signed tx 1 (conflicting tx)")

        # Broadcast the two tx's via the other node
        ds_txid = self.nodes[1].sendrawtransaction(ds_txs[0])
        # Gobble up the two zmq notifs for hashtx and verify them again
        txid = self.hashtx.receive()
        # Should receive the broadcasted raw transaction.
        hex = self.rawtx.receive()
        assert_equal(ds_txid, hash256_reversed(hex).hex())
        assert_equal(ds_txid, txid.hex())
        # this is normal, it gets rejected as an attempted double-spend
        assert_raises_rpc_error(
            -26,
            "txn-mempool-conflict (code 18)",
            self.nodes[1].sendrawtransaction,
            ds_txs[1]
        )
        self.sync_all()

        # Should receive the in-mempool txid as a double-spend notification
        self.log.info("Receiving hashds")
        ds_txid_zmq: bytes = self.hashds.receive()
        assert_equal(ds_txid, ds_txid_zmq.hex())

        # Should also receive the raw double-spent tx data
        self.log.info("Receiving rawds")
        ds_tx_zmq: bytes = self.rawds.receive()
        assert_equal(ds_txs[0], ds_tx_zmq.hex())


if __name__ == '__main__':
    ZMQTest().main()
