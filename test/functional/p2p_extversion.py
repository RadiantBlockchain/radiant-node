#!/usr/bin/env python3
# Copyright (c) 2015-2020 The Bitcoin Unlimited developers
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import sys
if sys.version_info[0] < 3:
    sys.exit("Use Python 3")
from test_framework.test_framework import BitcoinTestFramework
from test_framework.p2p import P2PInterface, p2p_lock
from test_framework.messages import NODE_NETWORK, msg_version, msg_extversion, msg_verack, MIN_VERSION_SUPPORTED, NODE_EXTVERSION
from test_framework.util import assert_equal, wait_until


class P2PIgnoreInv(P2PInterface):
    firstAddrnServices = 0

    def on_addr(self, message):
        self.firstAddrnServices = message.addrs[0].nServices


class BaseNode(P2PInterface):
    def __init__(self):
        super().__init__()
        self.version_received = False
        self.extversion_received = False
        self.verack_received = False
        self.remote_extversion = None

    def peer_connect(self, *args, services=NODE_NETWORK,
                     send_version=True, **kwargs):
        create_conn = super().peer_connect(*args, **kwargs)
        if send_version:
            # Send a version msg
            vt = msg_version()
            services = services | (1 << 11)
            vt.nServices = services
            vt.addrTo.ip = self.dstaddr
            vt.addrTo.port = self.dstport
            vt.addrFrom.ip = "0.0.0.0"
            vt.addrFrom.port = 0
            # Will be sent soon after connection_made
            self.on_connection_send_msg = vt
            print("sent version")
            return create_conn

    def send_extversion(self, xmap: dict):
        self.send_message(msg_extversion(xmap))
        print("sent extversion")

    def send_verack(self):
        self.send_message(msg_verack())
        print("sent verack")

    def on_version(self, message):
        assert message.nVersion >= MIN_VERSION_SUPPORTED, "Version {} received. Test framework only supports versions greater than {}".format(
            message.nVersion, MIN_VERSION_SUPPORTED)
        self.nServices = message.nServices
        self.version_received = True
        print("received version")

    def on_extversion(self, message):
        self.extversion_received = True
        self.remote_extversion = message
        print("received extversion")

    def on_verack(self, message):
        self.verack_received = True
        print("received verack")

    def wait_for_version(self, timeout=5):
        def test_function(): return self.version_received == True
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_extversion(self, timeout=5):
        def test_function(): return self.extversion_received == True
        wait_until(test_function, timeout=timeout, lock=p2p_lock)

    def wait_for_verack(self, timeout=5):
        def test_function(): return self.verack_received == True
        wait_until(test_function, timeout=timeout, lock=p2p_lock)


class ExtVersionTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # Node 0 has been started with extversion disabled by default. Check service bit.
        check_node = self.nodes[0].add_p2p_connection(P2PIgnoreInv())
        self.log.info("Check that node started with default is not signaling NODE_EXTVERSION.")
        assert_equal(check_node.nServices & NODE_EXTVERSION, 0)

        self.log.info("Check that getnetworkinfo does not show NODE_EXTVERSION service bit by default.")
        assert_equal(int(self.nodes[0].getnetworkinfo()[
                     'localservices'], 16) & NODE_EXTVERSION, 0)

        # Restart node 0 with extversion enabled.
        self.stop_nodes()
        extra_args = [["-useextversion"]]
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)

        check_node = self.nodes[0].add_p2p_connection(P2PIgnoreInv())
        self.log.info("Check that node started with -useextversion is signaling NODE_EXTVERSION.")
        assert_equal(check_node.nServices & NODE_EXTVERSION, NODE_EXTVERSION)

        self.log.info("Check that getnetworkinfo shows NODE_EXTVERSION service bit when -useextversion.")
        assert_equal(int(self.nodes[0].getnetworkinfo()[
                     'localservices'], 16) & NODE_EXTVERSION, NODE_EXTVERSION)

        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode

        # Test regular setup including extversion, standard version 0.1.0
        self.log.info("Testing node parses our extversion as 0.1.0")
        conn.wait_for_version()
        conn.wait_for_extversion()
        with self.nodes[0].assert_debug_log(["is using extversion 0.1.0"]):
            conn.send_extversion({
                0: 100,  # standard version: 0.1.0
            })
        conn.wait_for_verack()
        conn.send_verack()
        # Make sure extversion has actually been received properly
        self.log.info("Testing node's extversion matches what we expect")
        assert 0 in conn.remote_extversion.xver
        ver = int.from_bytes(conn.remote_extversion.xver[0], byteorder="little", signed=False)
        # we expect peer to have version 0.1.0 -> which is represented as the int 100
        assert_equal(ver, 100)

        self.stop_nodes()

        # Test regular setup including extversion with huge int values
        self.log.info("Testing extversion with huge int values")
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)
        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode
        conn.wait_for_version()
        conn.wait_for_extversion()
        with self.nodes[0].assert_debug_log(["is using extversion 3953421312.99.67"]):
            conn.send_extversion({
                0: 3_953_421_312 * 10_000 + 9900 + 67,  # arbitrary version: 3953421312.99.67
                111_989_234_231_321_325: b"random unknown data",  # an unknown huge key
                48_325: b"some other data",  # another unknown huge key
            })
        conn.wait_for_verack()
        conn.send_verack()

        self.stop_nodes()

        # Test versionbit mismatch
        self.log.info("Testing extversion service bit mismatch")
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)
        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode
        conn.wait_for_version()
        conn.wait_for_extversion()
        # if we send verack instead of extversion we should get a verack response
        conn.send_verack()
        conn.wait_for_verack()

        self.stop_nodes()

        # Test fatal versionbit handshake error
        self.log.info("Testing extversion handshake error")
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)
        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode
        conn.wait_for_version()
        conn.wait_for_extversion()
        # if we send verack instead of extversion we should get a verack response
        conn.send_verack()
        conn.wait_for_verack()
        with self.nodes[0].assert_debug_log(["received extversion message after verack, disconnecting"]):
            # Now, try sending extversion late -- this should cause an immediate disconnect
            conn.send_extversion({})
        conn.wait_for_disconnect(timeout=10.0)

        self.stop_nodes()

        # Test missing extversion complaint in log
        self.log.info("Testing extversion missing log message")
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)
        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode
        conn.wait_for_version()
        conn.wait_for_extversion()
        with self.nodes[0].assert_debug_log(["did not send us their \"Version\" key"]):
            conn.send_extversion({})  # empty map, no data
        conn.wait_for_verack()
        conn.send_verack()

        self.stop_nodes()

        # Test xmap message too large in log
        self.log.info("Testing extversion huge message rejection")
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)
        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode
        conn.wait_for_version()
        conn.wait_for_extversion()
        LIMIT = 100_000
        with self.nodes[0].assert_debug_log([f"An extversion message xmap must not exceed {LIMIT} bytes"]):
            conn.send_extversion({1234: b'z' * (LIMIT + 1)})  # huge data size

        self.stop_nodes()

        # Test xmap too many keys
        self.log.info("Testing extversion too many keys rejection")
        self.start_nodes(extra_args)
        self.nodes[0].clearbanned(manual=True, automatic=True)
        self.pynode = pynode = BaseNode()
        self.nodes[0].add_p2p_connection(pynode, send_version=True, wait_for_verack=False)
        conn = pynode
        conn.wait_for_version()
        conn.wait_for_extversion()
        LIMIT_KEYS = 8192
        with self.nodes[0].assert_debug_log(["size too large"]):
            conn.send_extversion({i: b'' for i in range(LIMIT_KEYS + 1)})  # too many keys

        self.stop_nodes()


if __name__ == '__main__':
    ExtVersionTest().main()
