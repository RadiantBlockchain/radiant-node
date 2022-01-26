#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) 2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""Test RPCs related to blockchainstate.

Test the following RPCs:
    - getblockchaininfo
    - gettxoutsetinfo
    - getdifficulty
    - getbestblockhash
    - getblockhash
    - getblockheader
    - getchaintxstats
    - getnetworkhashps
    - verifychain

Tests correspond to code in rpc/blockchain.cpp.
"""

from decimal import Decimal
import http.client
import subprocess
import string
from io import BytesIO

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_greater_than_or_equal,
    assert_raises,
    assert_raises_rpc_error,
    assert_is_hash_string,
    assert_is_hex_string,
    hex_str_to_bytes,
)
from test_framework.blocktools import (
    create_block,
    create_coinbase,
)
from test_framework.messages import (
    CBlockHeader,
    msg_block,
)
from test_framework.p2p import (
    P2PInterface,
)


class BlockchainTest(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1

    def run_test(self):
        # Set extra args with pruning after rescan is complete
        self.restart_node(0, extra_args=['-stopatheight=207', '-prune=1'])

        self._test_getblockchaininfo()
        self._test_getchaintxstats()
        self._test_gettxoutsetinfo()
        self._test_getblockheader()
        self._test_getdifficulty()
        self._test_getnetworkhashps()
        self._test_stopatheight()
        self._test_waitforblockheight()
        if self.is_wallet_compiled():
            self._test_getblock()
        assert self.nodes[0].verifychain(4, 0)

    def _test_getblockchaininfo(self):
        self.log.info("Test getblockchaininfo")

        keys = [
            'bestblockhash',
            'blocks',
            'chain',
            'chainwork',
            'difficulty',
            'headers',
            'initialblockdownload',
            'mediantime',
            'pruned',
            'size_on_disk',
            'verificationprogress',
            'warnings',
        ]
        res = self.nodes[0].getblockchaininfo()

        # result should have these additional pruning keys if manual pruning is
        # enabled
        assert_equal(sorted(res.keys()), sorted(
            ['pruneheight', 'automatic_pruning'] + keys))

        # size_on_disk should be > 0
        assert_greater_than(res['size_on_disk'], 0)

        # pruneheight should be greater or equal to 0
        assert_greater_than_or_equal(res['pruneheight'], 0)

        # check other pruning fields given that prune=1
        assert res['pruned']
        assert not res['automatic_pruning']

        self.restart_node(0, ['-stopatheight=207'])
        res = self.nodes[0].getblockchaininfo()
        # should have exact keys
        assert_equal(sorted(res.keys()), keys)

        self.restart_node(0, ['-stopatheight=207', '-prune=550'])
        res = self.nodes[0].getblockchaininfo()
        # result should have these additional pruning keys if prune=550
        assert_equal(sorted(res.keys()), sorted(
            ['pruneheight', 'automatic_pruning', 'prune_target_size'] + keys))

        # check related fields
        assert res['pruned']
        assert_equal(res['pruneheight'], 0)
        assert res['automatic_pruning']
        assert_equal(res['prune_target_size'], 576716800)
        assert_greater_than(res['size_on_disk'], 0)

    def _test_getchaintxstats(self):
        self.log.info("Test getchaintxstats")

        # Test `getchaintxstats` invalid extra parameters
        assert_raises_rpc_error(
            -1, 'getchaintxstats', self.nodes[0].getchaintxstats, 0, '', 0)

        # Test `getchaintxstats` invalid `nblocks`
        assert_raises_rpc_error(
            -1, "JSON value is not an integer as expected", self.nodes[0].getchaintxstats, '')
        assert_raises_rpc_error(
            -8, "Invalid block count: should be between 0 and the block's height - 1", self.nodes[0].getchaintxstats, -1)
        assert_raises_rpc_error(-8, "Invalid block count: should be between 0 and the block's height - 1", self.nodes[
                                0].getchaintxstats, self.nodes[0].getblockcount())

        # Test `getchaintxstats` invalid `blockhash`
        assert_raises_rpc_error(
            -1, "JSON value is not a string as expected", self.nodes[0].getchaintxstats, blockhash=0)
        assert_raises_rpc_error(-8,
                                "blockhash must be of length 64 (not 1, for '0')",
                                self.nodes[0].getchaintxstats,
                                blockhash='0')
        assert_raises_rpc_error(
            -8,
            "blockhash must be hexadecimal string (not 'ZZZ0000000000000000000000000000000000000000000000000000000000000')",
            self.nodes[0].getchaintxstats,
            blockhash='ZZZ0000000000000000000000000000000000000000000000000000000000000')
        assert_raises_rpc_error(
            -5,
            "Block not found",
            self.nodes[0].getchaintxstats,
            blockhash='0000000000000000000000000000000000000000000000000000000000000000')
        blockhash = self.nodes[0].getblockhash(200)
        self.nodes[0].invalidateblock(blockhash)
        assert_raises_rpc_error(
            -8, "Block is not in main chain", self.nodes[0].getchaintxstats, blockhash=blockhash)
        self.nodes[0].reconsiderblock(blockhash)

        chaintxstats = self.nodes[0].getchaintxstats(nblocks=1)
        # 200 txs plus genesis tx
        assert_equal(chaintxstats['txcount'], 201)
        # tx rate should be 1 per 10 minutes, or 1/600
        # we have to round because of binary math
        assert_equal(round(chaintxstats['txrate'] * 600, 10), Decimal(1))

        b1_hash = self.nodes[0].getblockhash(1)
        b1 = self.nodes[0].getblock(b1_hash)
        b200_hash = self.nodes[0].getblockhash(200)
        b200 = self.nodes[0].getblock(b200_hash)
        time_diff = b200['mediantime'] - b1['mediantime']

        chaintxstats = self.nodes[0].getchaintxstats()
        assert_equal(chaintxstats['time'], b200['time'])
        assert_equal(chaintxstats['txcount'], 201)
        assert_equal(chaintxstats['window_final_block_hash'], b200_hash)
        assert_equal(chaintxstats['window_block_count'], 199)
        assert_equal(chaintxstats['window_tx_count'], 199)
        assert_equal(chaintxstats['window_interval'], time_diff)
        assert_equal(
            round(chaintxstats['txrate'] * time_diff, 10), Decimal(199))

        chaintxstats = self.nodes[0].getchaintxstats(blockhash=b1_hash)
        assert_equal(chaintxstats['time'], b1['time'])
        assert_equal(chaintxstats['txcount'], 2)
        assert_equal(chaintxstats['window_final_block_hash'], b1_hash)
        assert_equal(chaintxstats['window_block_count'], 0)
        assert 'window_tx_count' not in chaintxstats
        assert 'window_interval' not in chaintxstats
        assert 'txrate' not in chaintxstats

    def _test_gettxoutsetinfo(self):
        node = self.nodes[0]
        res = node.gettxoutsetinfo()

        assert_equal(res['total_amount'], Decimal('8725.00000000'))
        assert_equal(res['transactions'], 200)
        assert_equal(res['height'], 200)
        assert_equal(res['txouts'], 200)
        assert_equal(res['bogosize'], 15000),
        assert_equal(res['bestblock'], node.getblockhash(200))
        size = res['disk_size']
        assert size > 6400
        assert size < 64000
        assert_equal(len(res['bestblock']), 64)
        assert_equal(len(res['hash_serialized']), 64)

        self.log.info(
            "Test that gettxoutsetinfo() works for blockchain with just the genesis block")
        b1hash = node.getblockhash(1)
        node.invalidateblock(b1hash)

        res2 = node.gettxoutsetinfo()
        assert_equal(res2['transactions'], 0)
        assert_equal(res2['total_amount'], Decimal('0'))
        assert_equal(res2['height'], 0)
        assert_equal(res2['txouts'], 0)
        assert_equal(res2['bogosize'], 0),
        assert_equal(res2['bestblock'], node.getblockhash(0))
        assert_equal(len(res2['hash_serialized']), 64)

        self.log.info(
            "Test that gettxoutsetinfo() returns the same result after invalidate/reconsider block")
        node.reconsiderblock(b1hash)

        res3 = node.gettxoutsetinfo()
        assert_equal(res['total_amount'], res3['total_amount'])
        assert_equal(res['transactions'], res3['transactions'])
        assert_equal(res['height'], res3['height'])
        assert_equal(res['txouts'], res3['txouts'])
        assert_equal(res['bogosize'], res3['bogosize'])
        assert_equal(res['bestblock'], res3['bestblock'])
        assert_equal(res['hash_serialized'], res3['hash_serialized'])

    def _test_getblockheader(self):
        node = self.nodes[0]

        assert_raises_rpc_error(-8,
                                "hash_or_height must be of length 64 (not 8, for 'nonsense')",
                                node.getblockheader,
                                "nonsense")
        assert_raises_rpc_error(
            -8,
            "hash_or_height must be hexadecimal string (not 'ZZZ7bb8b1697ea987f3b223ba7819250cae33efacb068d23dc24859824a77844')",
            node.getblockheader,
            "ZZZ7bb8b1697ea987f3b223ba7819250cae33efacb068d23dc24859824a77844")
        assert_raises_rpc_error(-5, "Block not found", node.getblockheader,
                                "0cf7bb8b1697ea987f3b223ba7819250cae33efacb068d23dc24859824a77844")
        assert_raises_rpc_error(
            -8,
            "Target block height 201 after current tip 200",
            node.getblockheader,
            201)
        assert_raises_rpc_error(
            -8,
            "Target block height -10 is negative",
            node.getblockheader,
            -10)

        besthash = node.getbestblockhash()
        secondbesthash = node.getblockhash(199)
        header = node.getblockheader(hash_or_height=besthash)

        assert_equal(header['hash'], besthash)
        assert_equal(header['height'], 200)
        assert_equal(header['confirmations'], 1)
        assert_equal(header['previousblockhash'], secondbesthash)
        assert_is_hex_string(header['chainwork'])
        assert_equal(header['nTx'], 1)
        assert_is_hash_string(header['hash'])
        assert_is_hash_string(header['previousblockhash'])
        assert_is_hash_string(header['merkleroot'])
        assert_is_hash_string(header['bits'], length=None)
        assert isinstance(header['time'], int)
        assert isinstance(header['mediantime'], int)
        assert isinstance(header['nonce'], int)
        assert isinstance(header['version'], int)
        assert isinstance(int(header['versionHex'], 16), int)
        assert isinstance(header['difficulty'], Decimal)

        header_by_height = node.getblockheader(hash_or_height=200)
        assert_equal(header, header_by_height)

        # Next, check that the old alias 'blockhash' still works
        # and is interchangeable with hash_or_height
        # First, make sure errors work as expected for unknown named params
        self.log.info("Testing that getblockheader(blockhashhh=\"HEX\") produces the proper error")
        assert_raises_rpc_error(
            -8,
            "Unknown named parameter blockhashhh",
            node.getblockheader,
            blockhashhh=header['hash'])
        # Next, actually try the old legacy blockhash="xx" style arg
        self.log.info("Testing that legacy getblockheader(blockhash=\"HEX\") still works ok")
        header_by_hash2 = node.getblockheader(blockhash=header['hash'])
        assert_equal(header, header_by_hash2)
        header_by_height2 = node.getblockheader(blockhash=200)
        assert_equal(header, header_by_height2)

        # check that we actually get a hex string back from getblockheader
        # if verbose is set to false.
        header_verbose_false = node.getblockheader(200, False)
        assert not isinstance(header_verbose_false, dict)
        assert isinstance(header_verbose_false, str)
        assert (c in string.hexdigits for c in header_verbose_false)
        assert_is_hex_string(header_verbose_false)

        # check that header_verbose_false is the same header we get via
        # getblockheader(hash_or_height=besthash) just in a different "form"
        h = CBlockHeader()
        h.deserialize(BytesIO(hex_str_to_bytes(header_verbose_false)))
        h.calc_sha256()

        assert_equal(header['version'], h.nVersion)
        assert_equal(header['time'], h.nTime)
        assert_equal(header['previousblockhash'], "{:064x}".format(h.hashPrevBlock))
        assert_equal(header['merkleroot'], "{:064x}".format(h.hashMerkleRoot))
        assert_equal(header['hash'], h.hash)

        # check that we get the same header by hash and by height in
        # the case verbose is set to False
        header_verbose_false_by_hash = node.getblockheader(besthash, False)
        assert_equal(header_verbose_false_by_hash, header_verbose_false)

    def _test_getdifficulty(self):
        difficulty = self.nodes[0].getdifficulty()
        # 1 hash in 2 should be valid, so difficulty should be 1/2**31
        # binary => decimal => binary math is why we do this check
        assert abs(difficulty * 2**31 - 1) < 0.0001

    def _test_getnetworkhashps(self):
        hashes_per_second = self.nodes[0].getnetworkhashps()
        # This should be 2 hashes every 10 minutes or 1/300
        assert abs(hashes_per_second * 300 - 1) < 0.0001
        assert_equal(hashes_per_second, self.nodes[0].getnetworkhashps(None, self.nodes[0].getblockcount()))
        for not_positive in (-1, 0):
            assert_raises_rpc_error(
                -8,
                "Number of blocks must be positive (using blocks since last difficulty change is no longer possible, because difficulty changes every block)",
                self.nodes[0].getnetworkhashps,
                not_positive)

    def _test_stopatheight(self):
        assert_equal(self.nodes[0].getblockcount(), 200)
        self.generatetoaddress(self.nodes[0],
                               6, self.nodes[0].get_deterministic_priv_key().address)
        assert_equal(self.nodes[0].getblockcount(), 206)
        self.log.debug('Node should not stop at this height')
        assert_raises(subprocess.TimeoutExpired,
                      lambda: self.nodes[0].process.wait(timeout=3))
        try:
            self.generatetoaddress(self.nodes[0],
                                   1, self.nodes[0].get_deterministic_priv_key().address)
        except (ConnectionError, http.client.BadStatusLine):
            pass  # The node already shut down before response
        self.log.debug('Node should stop at this height...')
        self.nodes[0].wait_until_stopped()
        self.start_node(0)
        assert_equal(self.nodes[0].getblockcount(), 207)

    def _test_waitforblockheight(self):
        self.log.info("Test waitforblockheight")
        node = self.nodes[0]
        node.add_p2p_connection(P2PInterface())

        current_height = node.getblock(node.getbestblockhash())['height']

        # Create a fork somewhere below our current height, invalidate the tip
        # of that fork, and then ensure that waitforblockheight still
        # works as expected.
        #
        # (Previously this was broken based on setting
        # `rpc/blockchain.cpp:latestblock` incorrectly.)
        #
        b20hash = node.getblockhash(20)
        b20 = node.getblock(b20hash)

        def solve_and_send_block(prevhash, height, time):
            b = create_block(prevhash, create_coinbase(height), time)
            b.solve()
            node.p2p.send_and_ping(msg_block(b))
            return b

        b21f = solve_and_send_block(int(b20hash, 16), 21, b20['time'] + 1)
        b22f = solve_and_send_block(b21f.sha256, 22, b21f.nTime + 1)

        node.invalidateblock(b22f.hash)

        def assert_waitforheight(height, timeout=2):
            assert_equal(
                node.waitforblockheight(
                    height=height, timeout=timeout)['height'],
                current_height)

        assert_waitforheight(0)
        assert_waitforheight(current_height - 1)
        assert_waitforheight(current_height)
        assert_waitforheight(current_height + 1)

    def _test_getblock(self):
        # Checks for getblock verbose outputs
        node = self.nodes[0]
        blockinfo = node.getblock(node.getblockhash(1), 2)
        transactioninfo = node.gettransaction(blockinfo['tx'][0]['txid'])
        blockheaderinfo = node.getblockheader(node.getblockhash(1), True)

        assert_equal(blockinfo['hash'], transactioninfo['blockhash'])
        assert_equal(
            blockinfo['confirmations'],
            transactioninfo['confirmations'])
        assert_equal(blockinfo['height'], blockheaderinfo['height'])
        assert_equal(blockinfo['versionHex'], blockheaderinfo['versionHex'])
        assert_equal(blockinfo['version'], blockheaderinfo['version'])
        assert_equal(blockinfo['size'], 181)
        assert_equal(blockinfo['merkleroot'], blockheaderinfo['merkleroot'])
        # Verify transaction data by check the hex values
        for tx in blockinfo['tx']:
            rawtransaction = node.getrawtransaction(tx['txid'], True)
            assert_equal(tx['hex'], rawtransaction['hex'])
        assert_equal(blockinfo['time'], blockheaderinfo['time'])
        assert_equal(blockinfo['mediantime'], blockheaderinfo['mediantime'])
        assert_equal(blockinfo['nonce'], blockheaderinfo['nonce'])
        assert_equal(blockinfo['bits'], blockheaderinfo['bits'])
        assert_equal(blockinfo['difficulty'], blockheaderinfo['difficulty'])
        assert_equal(blockinfo['chainwork'], blockheaderinfo['chainwork'])
        assert_equal(
            blockinfo['previousblockhash'],
            blockheaderinfo['previousblockhash'])
        assert_equal(
            blockinfo['nextblockhash'],
            blockheaderinfo['nextblockhash'])


if __name__ == '__main__':
    BlockchainTest().main()
