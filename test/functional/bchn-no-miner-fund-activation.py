#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Test derived from the corresponding abc-miner-fund test in Bitcoin ABC 0.21.
Assertions added/modified from the original test in Bitcoin ABC:
* -enableminerfund is not a valid configuration option.
* By default we do not enable the lowest four version bits prior to axion activation.
* We do not track BIP9 voting on the IFP.
* We still mine without miner fund contributions after the IFP activated in ABC.
* We still accept non-contributing blocks after the IFP activated in ABC.
"""

from test_framework.blocktools import (create_block, create_coinbase)
from test_framework.messages import ToHex
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
)

AXION_ACTIVATION_TIME = 1605441600
VERSION_BASE = 536870912

MINER_FUND_RATIO = 20

MINER_FUND_ADDR = 'bchreg:pqv2r67sgz3qumufap3h2uuj0zfmnzuv8v7ej0fffv'
MINER_FUND_ABC_ADDR = 'bchreg:qzvz0es48sf8wrqy7kn5j5cugka95ztskcra2r7ee7'
MINER_FUND_BCHD_ADDR = 'bchreg:qrhea03074073ff3zv9whh0nggxc7k03ssffq2ylju'
MINER_FUND_ELECTRON_CASH_ADDR = 'bchreg:pp8d685l8kecnmtyy52ndvq625arz2qwmutyjlcyav'


class MinerFundTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1

    def check_bip9_state(self, name, status):
        assert_equal(False, 'softforks' in self.nodes[0].getblockchaininfo())

    def run_test(self):
        node = self.nodes[0]
        address = node.get_deterministic_priv_key().address

        # Get the vote started.
        node.setmocktime(1580000000)

        def check_all_bip9_state(status):
            self.check_bip9_state('minerfund', status)
            self.check_bip9_state('minerfundabc', status)
            self.check_bip9_state('minerfundbchd', status)
            self.check_bip9_state('minerfundelectroncash', status)

        # We need to finish the current period and move the MTP forward.
        while ((node.getblockchaininfo()['blocks'] + 2) % 144) != 0:
            node.generatetoaddress(1, address)
            check_all_bip9_state({
                'status': 'defined',
                'start_time': 1573819200,
                'timeout': 1589544000,
                'since': 0,
            })

        height = node.getblockcount()
        self.run_no_miner_fund_test()

        def run_test_for(bit, name, address):
            # Make sure we have a clean slate.
            node.invalidateblock(node.getblockhash(height + 1))
            self.run_miner_fund_test(bit, name, address)

        run_test_for(0, 'minerfund', MINER_FUND_ADDR)
        run_test_for(1, 'minerfundabc', MINER_FUND_ABC_ADDR)
        run_test_for(2, 'minerfundbchd', MINER_FUND_BCHD_ADDR)
        run_test_for(3, 'minerfundelectroncash', MINER_FUND_ELECTRON_CASH_ADDR)

        # MAKE SURE THE MINER FUND CANNOT BE ENABLED.
        self.stop_node(0)
        node.assert_start_raises_init_error(
            ['-enableminerfund', "-axionactivationtime={}".format(AXION_ACTIVATION_TIME)], 'Error parsing command line arguments: Invalid parameter -enableminerfund')

    def run_no_miner_fund_test(self):
        node = self.nodes[0]
        address = node.get_deterministic_priv_key().address

        def get_best_vote():
            return node.getblockheader(node.getbestblockhash())['version'] & 0xF

        # Move MTP forward to axion activation WITHOUT VOTING FOR IFP.
        node.setmocktime(AXION_ACTIVATION_TIME)
        for i in range(6):
            node.generatetoaddress(1, address)
            assert_equal(get_best_vote(), 0)
        assert_equal(
            node.getblockchaininfo()['mediantime'],
            AXION_ACTIVATION_TIME)

        # First block with the new rules.
        node.generatetoaddress(1, address)

        def get_best_coinbase():
            return node.getblock(node.getbestblockhash(), 2)['tx'][0]

        # No money goes to the fund.
        coinbase = get_best_coinbase()
        assert_equal(len(coinbase['vout']), 1)

    def run_miner_fund_test(self, bit, name, fund_address):
        self.log.info("Testing miner fund {} on bit {}.".format(name, bit))

        version = VERSION_BASE | (1 << bit)

        self.stop_node(0)
        self.start_node(0,
                        ["-blockversion={}".format(version), "-axionactivationtime={}".format(AXION_ACTIVATION_TIME)])

        node = self.nodes[0]
        node.setmocktime(1580000000)
        address = node.get_deterministic_priv_key().address

        def get_best_vote():
            return node.getblockheader(node.getbestblockhash())['version'] & 0xF

        for i in range(144):
            node.generatetoaddress(1, address)
            assert_equal(get_best_vote(), 1 << bit)
            self.check_bip9_state(name, {
                'status': 'started',
                'bit': bit,
                'start_time': 1573819200,
                'timeout': 1589544000,
                'since': 288,
                'statistics': {
                    'period': 144,
                    'threshold': 96,
                    'elapsed': i,
                    'count': i,
                    'possible': True,
                },
            })

        for i in range(144):
            node.generatetoaddress(1, address)
            assert_equal(get_best_vote(), 1 << bit)
            self.check_bip9_state(name, {
                'status': 'locked_in',
                'start_time': 1573819200,
                'timeout': 1589544000,
                'since': 432,
            })

        # Now this should be active.
        node.generatetoaddress(1, address)
        assert_equal(get_best_vote(), 1 << bit)
        self.check_bip9_state(name, {
            'status': 'active',
            'start_time': 1573819200,
            'timeout': 1589544000,
            'since': 576,
        })

        # Move MTP forward to axion activation.
        node.setmocktime(AXION_ACTIVATION_TIME)
        for i in range(6):
            node.generatetoaddress(1, address)
            assert_equal(get_best_vote(), 1 << bit)
        assert_equal(
            node.getblockchaininfo()['mediantime'],
            AXION_ACTIVATION_TIME)

        self.check_bip9_state(name, {
            'status': 'active',
            'start_time': 1573819200,
            'timeout': 1589544000,
            'since': 576,
        })

        # Let's remember the hash of this block for later use.
        fork_block_hash = int(node.getbestblockhash(), 16)

        def get_best_coinbase():
            return node.getblock(node.getbestblockhash(), 2)['tx'][0]

        # Check that we do not send anything to the fund yet.
        assert_equal(len(get_best_coinbase()['vout']), 1)

        # Now the miner fund is STILL NOT enforced
        node.generatetoaddress(1, address)

        # Now we STILL DO NOT send part of the coinbase to the fund.
        coinbase = get_best_coinbase()
        assert_equal(len(coinbase['vout']), 1)
        assert_equal(False, coinbase['vout'][0]['scriptPubKey']['addresses'][0] == fund_address)

        # Invalidate top block, submit a custom block that do not send anything
        # to the fund and check it is NOT rejected.
        node.invalidateblock(node.getbestblockhash())

        block_height = node.getblockcount()
        block = create_block(
            fork_block_hash, create_coinbase(block_height), AXION_ACTIVATION_TIME + 99)
        block.solve()

        assert_equal(node.submitblock(ToHex(block)), None)


if __name__ == '__main__':
    MinerFundTest().main()
