#!/usr/bin/env python3
# Copyright (c) 2015-2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test node responses to invalid network messages."""
import asyncio
import struct

from test_framework.messages import (
    CBlockHeader,
    CInv,
    msg_getdata,
    msg_headers,
    msg_inv,
    msg_ping,
    MSG_TX,
    ser_string,
)
from test_framework.mininode import (
    NetworkThread,
    P2PDataStore,
    P2PInterface,
)
from test_framework.test_framework import BitcoinTestFramework


class msg_unrecognized:
    """Nonsensical message. Modeled after similar types in test_framework.messages."""

    command = b'badmsg'

    def __init__(self, *, str_data):
        self.str_data = str_data.encode() if not isinstance(str_data, bytes) else str_data

    def serialize(self):
        return ser_string(self.str_data)

    def __repr__(self):
        return "{}(data={})".format(self.command, self.str_data)


class InvalidMessagesTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.setup_clean_chain = True

    def run_test(self):
        """
         . Test msg header
        0. Send a bunch of large (2MB) messages of an unrecognized type. Check to see
           that it isn't an effective DoS against the node.
        """
        self.test_magic_bytes()
        self.test_checksum()
        self.test_size()
        self.test_command()
        self.test_large_inv()

        node = self.nodes[0]
        self.node = node
        node.add_p2p_connection(P2PDataStore())
        conn2 = node.add_p2p_connection(P2PDataStore())

        # 2MB, per MAX_PROTOCOL_MESSAGE_LENGTH
        msg_limit = 2 * 1024 * 1024
        # Account for the 4-byte length prefix
        valid_data_limit = msg_limit - 5

        #
        # 0.
        #
        # Send as large a message as is valid, ensure we aren't disconnected but
        # also can't exhaust resources.
        #
        msg_at_size = msg_unrecognized(str_data="b" * valid_data_limit)
        assert len(msg_at_size.serialize()) == msg_limit

        self.log.info(
            "Sending a bunch of large, junk messages to test memory exhaustion. May take a bit...")

        # Run a bunch of times to test for memory exhaustion.
        for _ in range(80):
            node.p2p.send_message(msg_at_size)

        # Check that, even though the node is being hammered by nonsense from one
        # connection, it can still service other peers in a timely way.
        for _ in range(20):
            conn2.sync_with_ping(timeout=2)

        # Peer 1, despite serving up a bunch of nonsense, should still be
        # connected.
        self.log.info("Waiting for node to drop junk messages.")
        node.p2p.sync_with_ping(timeout=320)
        assert node.p2p.is_connected

    def test_magic_bytes(self):
        conn = self.nodes[0].add_p2p_connection(P2PDataStore())

        async def swap_magic_bytes():
            # Need to ignore all incoming messages from now, since they come
            # with "invalid" magic bytes
            conn._on_data = lambda: None
            conn.magic_bytes = b'\x00\x11\x22\x32'

        # Call .result() to block until the atomic swap is complete, otherwise
        # we might run into races later on
        asyncio.run_coroutine_threadsafe(
            swap_magic_bytes(),
            NetworkThread.network_event_loop).result()

        with self.nodes[0].assert_debug_log(['PROCESSMESSAGE: INVALID MESSAGESTART ping']):
            conn.send_message(msg_ping(nonce=0xff))
            conn.wait_for_disconnect(timeout=1)
            self.nodes[0].disconnect_p2ps()

    def test_checksum(self):
        conn = self.nodes[0].add_p2p_connection(P2PDataStore())
        with self.nodes[0].assert_debug_log(['ProcessMessages(badmsg, 2 bytes): CHECKSUM ERROR expected 78df0a04 was ffffffff']):
            msg = conn.build_message(msg_unrecognized(str_data="d"))
            cut_len = (
                # magic
                4 +
                # command
                12 +
                # len
                4
            )
            # modify checksum
            msg = msg[:cut_len] + b'\xff' * 4 + msg[cut_len + 4:]
            self.nodes[0].p2p.send_raw_message(msg)
            conn.wait_for_disconnect()
            self.nodes[0].disconnect_p2ps()

    def test_size(self):
        conn = self.nodes[0].add_p2p_connection(P2PDataStore())
        with self.nodes[0].assert_debug_log(['']):
            msg = conn.build_message(msg_unrecognized(str_data="d"))
            cut_len = (
                # magic
                4 +
                # command
                12
            )
            # modify len to MAX_SIZE + 1
            msg = msg[:cut_len] + \
                struct.pack("<I", 0x02000000 + 1) + msg[cut_len + 4:]
            self.nodes[0].p2p.send_raw_message(msg)
            conn.wait_for_disconnect(timeout=1)
            self.nodes[0].disconnect_p2ps()

    def test_command(self):
        conn = self.nodes[0].add_p2p_connection(P2PDataStore())
        with self.nodes[0].assert_debug_log(['PROCESSMESSAGE: ERRORS IN HEADER']):
            msg = msg_unrecognized(str_data="d")
            msg.command = b'\xff' * 12
            msg = conn.build_message(msg)
            # Modify command
            msg = msg[:7] + b'\x00' + msg[7 + 1:]
            self.nodes[0].p2p.send_raw_message(msg)
            conn.sync_with_ping(timeout=1)
            self.nodes[0].disconnect_p2ps()

    def test_large_inv(self):
        conn = self.nodes[0].add_p2p_connection(P2PInterface())
        with self.nodes[0].assert_debug_log(['Misbehaving', 'peer=4 (0 -> 20) reason: oversized-inv']):
            msg = msg_inv([CInv(MSG_TX, 1)] * 50001)
            conn.send_and_ping(msg)
        with self.nodes[0].assert_debug_log(['Misbehaving', 'peer=4 (20 -> 40) reason: too-many-inv']):
            msg = msg_getdata([CInv(MSG_TX, 1)] * 50001)
            conn.send_and_ping(msg)
        with self.nodes[0].assert_debug_log(['Misbehaving', 'peer=4 (40 -> 60) reason: too-many-headers']):
            msg = msg_headers([CBlockHeader()] * 2001)
            conn.send_and_ping(msg)
        self.nodes[0].disconnect_p2ps()


if __name__ == '__main__':
    InvalidMessagesTest().main()
