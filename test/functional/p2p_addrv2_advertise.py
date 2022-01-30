#!/usr/bin/env python3
# Copyright (c) 2022 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
Test addrv2 advertisement
"""

from test_framework.messages import MY_VERSION
from test_framework.p2p import P2PInterface
from test_framework.test_framework import BitcoinTestFramework


class SpecificVersionP2PInterface(P2PInterface):
    sendaddrv2_received: bool = False
    version_override: int

    def __init__(self, version_override: int = MY_VERSION):
        super().__init__()
        self.version_override = version_override

    def peer_connect(self, *args, **kwargs):
        create_conn = super().peer_connect(*args, send_version=True, **kwargs)
        self.on_connection_send_msg.nVersion = self.version_override
        return create_conn

    def on_sendaddrv2(self, message):
        self.sendaddrv2_received = True


class AddrV2AdvertiseTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def test_version_70015_no_sendaddrv2(self):
        self.log.info('Create version 70015 connection and check that addrv2 is not advertised')
        receiver = self.nodes[0].add_p2p_connection(SpecificVersionP2PInterface(version_override=70015))
        receiver.wait_for_verack()
        assert not receiver.sendaddrv2_received

    def test_version_70016_sendaddrv2(self):
        self.log.info('Create version 70016 connection and check that addrv2 is advertised')
        receiver = self.nodes[0].add_p2p_connection(SpecificVersionP2PInterface(version_override=70016))
        receiver.wait_for_verack()
        assert receiver.sendaddrv2_received

    def run_test(self):
        self.test_version_70015_no_sendaddrv2()
        self.test_version_70016_sendaddrv2()


if __name__ == '__main__':
    AddrV2AdvertiseTest().main()
