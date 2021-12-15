#!/usr/bin/env python3
# Copyright (C) 2020 Calin Culianu and mtrycz
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests basic functionality of the getblocktemplatelight and submitblocklight RPC methods.
"""

import os
import platform
import random
import shutil
import stat
import tempfile
import time
from decimal import Decimal

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, assert_blocktemplate_equal
from test_framework import messages, script, util, blocktools


def to_bytes(x, encoding='utf8'):
    if isinstance(x, bytes):
        return x
    elif isinstance(x, str):
        return x.encode(encoding)
    elif isinstance(x, bytearray):
        return bytes(x)
    else:
        raise TypeError("Not a string or bytes-like object")


def hash160(b):
    b = to_bytes(b)
    return script.hash160(b)


def hash256(x):
    x = to_bytes(x)
    return messages.hash256(x)


def hex2bytes(x, rev=True):
    """ Decodes x and reverses its bytes if rev=True. If rev=False, no reversal is done.
    x can be a list in which case this operation is performed for each element recursively. """
    if isinstance(x, (str, bytes, bytearray)):
        if rev:
            return bytes.fromhex(x)[::-1]
        else:
            return bytes.fromhex(x)
    elif isinstance(x, (list, tuple)):
        return [hex2bytes(i, rev=rev) for i in x]
    raise TypeError('Unsupported type')


def bytes2hex(x, rev=True):
    """ Reverses x and returns its hex representation as a string. If rev=False, no reversal is done.
    x can be a list in which case this operation is performed for each element recursively. """
    if isinstance(x, (list, tuple)):
        return [bytes2hex(i, rev=rev) for i in x]
    else:
        if rev:
            return to_bytes(x)[::-1].hex()
        else:
            return to_bytes(x).hex()


def get_merkle_branch(hashes):
    """ See src/mining.cpp MakeMerkleBranch() for an explanation of this algorithm. """
    res = []
    if not hashes:
        return res
    assert isinstance(hashes[0], (bytes, bytearray))
    while len(hashes) > 1:
        res.append(hashes[0])  # take the first one
        if len(hashes) % 2 == 0:
            # enforce odd number of hashes
            hashes.append(hashes[-1])
        new_size = (len(hashes) - 1) // 2
        h = []
        for i in range(new_size):
            h.append(hash256(hashes[i * 2 + 1] + hashes[i * 2 + 2]))
        hashes = h
    assert len(hashes) == 1
    res.append(hashes[0])  # put the last one left
    return res


def merkle_root_from_cb_and_branch(cbhash, branch):
    """ Given a coinbase tx hash (bytes) and a merkle branch (list of bytes), calculate the merkle root.
    The merkle root is returned as a uint256 suitable for setting a CBlock.hashMerkleRoot """
    hashes = [cbhash] + branch
    while len(hashes) > 1:
        hashes[0] = hash256(hashes[0] + hashes[1])
        del hashes[1]
    return messages.uint256_from_str(hashes[0])  # this is now the root


class GBTLightTest(BitcoinTestFramework):
    """ Functional tests for the getblocktemplatelight and submitblocklight RPC methods. """

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2  # We need two nodes for getblocktemplatelight RPC to function (bitcoind node policy)
        self._cache_size = 10
        my_args = [
            # We specify a cache size to a known value for this test.
            '-gbtcachesize={}'.format(self._cache_size),
        ]
        self.extra_args = [my_args] * self.num_nodes
        # lastly, make node[1] have a custom -gbtstoredir to test that this arg takes effect
        self._custom_gbt_dir = tempfile.gettempdir()
        assert self._custom_gbt_dir
        uniq_suf = hash256(str(time.time()).encode('utf8')).hex()[:10]  # make a unique suffix based on the current time
        self._custom_gbt_dir = os.path.join(self._custom_gbt_dir, "gbt_{}".format(uniq_suf))
        self.extra_args[1].append(
            '-gbtstoredir={}'.format(self._custom_gbt_dir)
        )

    def check_job_id(self, gbtl):
        # check job_id is ok
        assert_equal(
            gbtl['job_id'],
            bytes2hex(hash160(hex2bytes(gbtl['previousblockhash']) + b''.join(hex2bytes(gbtl['merkle']))))
        )
        self.log.info("job_id ok!")

    def check_merkle(self, gbtl, txids):
        assert_equal(gbtl['merkle'], bytes2hex(get_merkle_branch(hex2bytes(sorted(txids)))))
        self.log.info("merkle ok!")

    min_relay_fee = 0

    def gen_valid_tx(self, node_num=0):
        """ Generate a single, valid, signed transaction using the wallet from node_num, and return its hex.
        This transaction is not submitted to mempool; it is simply generated, signed, and returned. """
        node = self.nodes[node_num]
        fee = self.min_relay_fee
        amount = Decimal("0.00002") * random.randrange(10)
        (total_in, inputs) = util.gather_inputs(node, amount + fee)
        outputs = util.make_change(node, total_in, amount, fee)
        outputs[node.getnewaddress()] = float(amount)
        rawtx = node.createrawtransaction(inputs, outputs)
        signresult = node.signrawtransactionwithwallet(rawtx)
        return signresult["hex"]

    def wait_for_txs(self, txids, nodenum=0, timeout=15):
        """ Wait up to timeout seconds for getblocktemplate's tx set to contain `txids`. """
        node = self.nodes[nodenum]
        end = time.time() + timeout
        txids = set(txids)
        while time.time() < end:
            txs = {x["txid"] for x in node.getblocktemplate()["transactions"]}
            if not (txids - txs):
                return
            time.sleep(0.250)
        raise RuntimeError("Timed out waiting for required txids from getblocktemplate")

    def set_mock_time(self, timeval):
        """ Enables mock time for all nodes.  Currently unused but here in case we need it. """
        for node in self.nodes:
            node.setmocktime(int(timeval))

    def clear_mock_time(self):
        """ Disables mock time for all nodes (reverting back to wall clock time).
        Currently unused but here in case we need it. """
        self.set_mock_time(0)  # 0 clears mock time

    def run_test(self):
        try:
            self.log.info("Node 1 is using custom -gbtstoredir: {}".format(self._custom_gbt_dir))
            self.__run_test()
            # We execute the test twice to test that after mining the block, all data structures (pblocktemplate, etc)
            # are properly reset and everything is sane on the C++-side.
            self.__run_test(nblocks_to_gen=0, ntx_to_gen=12, test_additional_txs=False)
        finally:
            # Unconditionally remove the custom gbtstoredir since the test framework doesn't know about it.
            shutil.rmtree(self._custom_gbt_dir)
            self.log.info("Cleaned-up custom -gbtstoredir: {}".format(self._custom_gbt_dir))

    def __run_test(self, *, nblocks_to_gen=150, ntx_to_gen=19, test_additional_txs=True):
        assert ntx_to_gen > 0
        # we will need this value for random_transaction below, and for self.gen_valid_tx
        self.min_relay_fee = self.nodes[0].getnetworkinfo()["relayfee"]

        if nblocks_to_gen > 0:
            if self.is_wallet_compiled():
                # generate some blocks to wallet to have spendable coins
                self.generate(self.nodes[0], nblocks_to_gen)
            else:
                # generate just 1 block to leave IBD state (no wallet so no spending in this mode)
                self.generatetoaddress(self.nodes[0], 1, self.nodes[0].get_deterministic_priv_key().address)

        self.sync_all()

        gbtl0 = self.nodes[0].getblocktemplatelight()
        gbtl1 = self.nodes[1].getblocktemplatelight()

        assert_blocktemplate_equal(gbtl0, gbtl1)

        def check_gbt_store_dir(gbtdir, job_id):
            expected_data_file = os.path.join(gbtdir, job_id)
            assert os.path.exists(gbtdir), "The -gbtstoredir must exist"
            assert os.path.exists(expected_data_file), "The -gbtstoredir must contain the expected job_id file"
        # check that node[1] is using the custom -gbtstoredir argument we gave it.
        check_gbt_store_dir(self._custom_gbt_dir, gbtl1['job_id'])

        self.check_job_id(gbtl0)
        self.check_merkle(gbtl0, [])  # empty merkle should be ok

        # generate a bunch of transactions
        txids = []
        ntx = ntx_to_gen if self.is_wallet_compiled() else 0
        for i in range(ntx):
            txid, txhex, fee = util.random_transaction((self.nodes[0],), Decimal("0.000123"),
                                                       self.min_relay_fee, Decimal("0.000001"), 0)
            txids.append(txid)

        # Since we have two nodes, sync the mempools
        self.sync_all()

        # Wait for getblocktemplate to see the txids (it uses a 5s caching strategy before it calculates a new template)
        # 'setmocktime' here worked too but we prefer to let the clock advance normally, rather than use a pegged
        # mocktime for this test case.  (Real execution time for this whole test case is about the same whether using
        # mocktime or this polling strategy, so better to keep time advancing normally).
        self.wait_for_txs(txids, 0)
        self.wait_for_txs(txids, 1)

        # Check that, once the nodes are synced, they give the same template
        gbtl0 = self.nodes[0].getblocktemplatelight()
        gbtl1 = self.nodes[1].getblocktemplatelight()
        assert_blocktemplate_equal(gbtl0, gbtl1)

        # check job_id is ok
        self.check_job_id(gbtl0)
        # check merkle is ok
        self.check_merkle(gbtl0, txids)

        if self.is_wallet_compiled() and test_additional_txs:
            # add the signed tx to a job.. we wil submit this later (only iff wallet enabled)
            signedtx = self.gen_valid_tx()
            signedtxid = bytes2hex(hash256(bytes.fromhex(signedtx)))
            self.log.info("Signed txid: {}  hex: {}".format(signedtxid, signedtx))
            gbtl0 = self.nodes[0].getblocktemplatelight({}, [signedtx])
            submit_job_id = gbtl0['job_id']
            submit_tmpl = gbtl0
            self.check_job_id(gbtl0)
            self.check_merkle(gbtl0, txids + [signedtxid])
        else:
            # No wallet (or caller wants to not test additional_tx).
            # Just use the last job with no additional_txs as the submit job
            submit_job_id, submit_tmpl = gbtl0['job_id'], gbtl0

        # These tx's are invalid on this chain, but they do at least deserialize correctly, so we can use them
        # to make a bunch of jobs
        extratxs = [
            "0100000002ae54229545be8d2738e245e7ea41d089fa3def0a48e9410b49f39ec43826971d010000006a4730440220204169229eb1"
            "7dc49ad83675d693e4012453db9a8d1af6f118278152c709f6be022077081ab76df0356e53c1ba26145a3fb98ca58553a98b1c130a"
            "2f6cff4d39767f412103cfbc58232f0761a828ced4ee93e87ce27f26d005dd9c87150aad5e5f07073dcaffffffff4eca0e441d0a27"
            "f874f41739382cb80fdf3aac0f7b8316e197dd42e7155590c1010000006a47304402203832a75ccfc2f12474c1d3d2fc22cd72cc92"
            "4c1b73995a27a0d07b9c5a745f3a022035d98e1017a4cb02ff1509d17c752047dca2b270b927793f2eb9e30af1ac02d6412103cfbc"
            "58232f0761a828ced4ee93e87ce27f26d005dd9c87150aad5e5f07073dcaffffffff0260ea00000000000017a9149eefc3ae114359"
            "8a830d66cbc32aa583fa3d987687fb030100000000001976a914bddb57be877bd32264fc40670b87b6fb271813f688ac00000000",

            "0100000001993b9740d3e289876cbe6920008a35c4a08b7dc4bd48ff61b198f163af3f354900000000644102a8588b2e1a808ade29"
            "4aa76a1e63137099fa087841603a361171f0c1473396f482d8d1a61e2d3ff94280b1125114868647bff822d2a74461c6bbe6ffc06f"
            "9d412102abaad90841057ddb1ed929608b536535b0cd8a18ba0a90dba66ba7b1c1f7b4eafeffffff0176942200000000001976a91"
            "40a373caf0ab3c2b46cd05625b8d545c295b93d7a88acf3fa1400",
        ]
        extratxids = bytes2hex([hash256(x) for x in hex2bytes(extratxs, rev=False)])

        # test "additional_txs"
        gbtl0 = self.nodes[0].getblocktemplatelight({}, extratxs)
        self.check_job_id(gbtl0)
        self.check_merkle(gbtl0, txids + extratxids)

        # test that the "additional_txs" didn't stick around in the cached pblocktemplate in getblocktemplatecommon
        gbtl0 = self.nodes[0].getblocktemplatelight({}, extratxs)
        self.check_merkle(gbtl0, txids + extratxids)
        gbt0 = self.nodes[0].getblocktemplate()
        assert_equal(sorted(txids), [x['txid'] for x in gbt0['transactions']])
        # try extratxs twice; they should both be present (known behavior)
        gbtl0 = self.nodes[0].getblocktemplatelight({}, extratxs + extratxs)
        self.check_merkle(gbtl0, txids + extratxids + extratxids)

        # try regular getblocktemplatelight again, without extratxs, test that extratxs didn't stick around
        gbtl0 = self.nodes[0].getblocktemplatelight()
        gbtl1 = self.nodes[1].getblocktemplatelight()
        assert_blocktemplate_equal(gbtl0, gbtl1)
        self.check_merkle(gbtl0, txids)

        # Test RPC errors

        # bad txn hex (decode failure) at index 1
        assert_raises_rpc_error(-22,
                                "additional_txs transaction 1 decode failure",
                                self.nodes[0].getblocktemplatelight,
                                {}, [extratxs[1], extratxs[0][:-15]])

        tmpl = submit_tmpl
        job_id = submit_job_id
        coinbase_tx = blocktools.create_coinbase(height=int(tmpl["height"]) + 1)
        coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
        coinbase_tx.rehash()

        block = messages.CBlock()
        block.nVersion = tmpl["version"]
        block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
        block.nTime = tmpl["curtime"]
        block.nBits = int(tmpl["bits"], 16)
        block.nNonce = 0
        block.vtx = [coinbase_tx]
        block.hashMerkleRoot = merkle_root_from_cb_and_branch(hash256(coinbase_tx.serialize()), hex2bytes(tmpl['merkle']))
        block.solve()

        # Be evil and attempt to send 2 tx's. Note that this code assumes the nTx check on the C++ side happens
        # before the merkle root check (which is the case currently).
        saved_vtx = block.vtx
        block.vtx = [coinbase_tx] * 2
        assert_raises_rpc_error(-22,
                                "Block must contain a single coinbase tx (light version)",
                                self.nodes[0].submitblocklight,
                                block.serialize().hex(), job_id)
        # swap it back to the correct value
        block.vtx = saved_vtx
        del saved_vtx

        # Test decode failure on bad block (this particular test is also done in the regular submitblock RPC tests
        # but we do it here too to be thorough)
        assert_raises_rpc_error(-22,
                                "Block decode failed",
                                self.nodes[0].submitblocklight,
                                block.serialize()[:-15].hex(), job_id)

        # Check bad job_id (not uint160 hex)
        bad_job_id = "abcdefxx123"
        assert_raises_rpc_error(-1,
                                "job_id must be a 40 character hexadecimal string (not '{bad}')".format(bad=bad_job_id),
                                self.nodes[0].submitblocklight,
                                block.serialize().hex(), bad_job_id)

        # Check unknown job_id error
        assert_raises_rpc_error(-8,
                                "job_id data not available",
                                self.nodes[0].submitblocklight,
                                block.serialize().hex(), "ab" * 20)

        def expire_in_memory_cache(num):
            """ Keeps creating new job_Ids so as to expire the in-memory cache for old jobs (default cache size: 10) """
            txs = extratxs * 3
            txi = extratxids * 3
            for i in range(num):
                tmpl = self.nodes[0].getblocktemplatelight({}, txs)
                self.check_job_id(tmpl)
                self.check_merkle(tmpl, txids + txi)
                txs += extratxs
                txi += extratxids
        expire_in_memory_cache(self._cache_size + 1)
        # at this point our submit_job_id's data will come from a file in the gbt/ dir, rather than the in-memory cache

        # And finally, actually submit the block using submitblocklight
        self.log.info("submitting for job_id: {} merkles: {}   HEX: {}"
                      .format(job_id, tmpl['merkle'], block.serialize().hex()))
        res = self.nodes[0].submitblocklight(block.serialize().hex(), job_id)
        self.log.info("submit result: {}".format(repr(res)))
        assert_equal(res, None)

        self.sync_all()

        # Test that block was in fact added to the blockchain and that both nodes see it
        blockHashHex = self.nodes[0].getbestblockhash()
        self.log.info("Accepted block hash: {}".format(blockHashHex))
        block.rehash()
        assert_equal(blockHashHex, block.hash)
        assert_equal(blockHashHex, self.nodes[1].getbestblockhash())

        # Final check -- check for proper RPC error when there is a problem writing out the job data file.
        # However, we can only do this simulated check on non-Windows, posix OS's, when we are not root.
        # Note that in most CI's we run as root, so this check will be skipped in CI.
        if not platform.system().lower().startswith('win') and os.name == 'posix' and os.getuid() != 0:
            orig_mode = None
            try:
                self.log.info("'simulated save failure' test will execute")
                orig_mode = os.stat(self._custom_gbt_dir).st_mode
                new_mode = orig_mode & ~(stat.S_IWUSR | stat.S_IWGRP | stat.S_IWOTH)
                # Set chmod of data directory to read-only to simulate an error writing to the job data file.
                # This should cause the anticipated error on the C++ side.
                os.chmod(self._custom_gbt_dir, new_mode)
                assert_raises_rpc_error(-32603,  # RPC_INTERNAL_ERROR
                                        "failed to save job tx data to disk",
                                        self.nodes[1].getblocktemplatelight,
                                        {}, extratxs)
            finally:
                if orig_mode is not None:
                    # undo the damage to the directory's mode from above
                    os.chmod(self._custom_gbt_dir, orig_mode)
        else:
            self.log.info("'simulated save failure' test skipped because either we are uid 0 or we are on Windows")


if __name__ == '__main__':
    GBTLightTest().main()
