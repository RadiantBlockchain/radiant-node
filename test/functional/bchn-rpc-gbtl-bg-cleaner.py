#!/usr/bin/env python3
# Copyright (C) 2020 Calin Culianu
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

"""
Tests that the getblocktemplatelight background 'cleaner' thread indeed
cleans expired data from the gbt/ directory.
"""

import os
import threading
import time

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal, assert_raises_rpc_error, wait_until, assert_blocktemplate_equal
from test_framework import messages, blocktools


class GBTLightBGCleanerTest(BitcoinTestFramework):
    """ Functional tests for the getblocktemplatelight background "cleaner" thread that removes data from the
    gbt/ directory.  We set the timeout to a short time and then observe job data first going to the trash/ folder
    and then being removed completely. """

    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 2  # We need connected nodes for getblocktemplatelight RPC to function (bitcoind node policy)
        self._cache_size = 5
        self._store_time = 10
        args = [
            '-gbtcachesize={}'.format(self._cache_size),
            '-gbtstoretime={}'.format(self._store_time),
        ]
        self.extra_args = [args] * self.num_nodes

    def run_test(self):
        # generate just 1 block to leave IBD state (no wallet is required for this test so we use hard-coded key)
        self.generatetoaddress(self.nodes[0], 1, self.nodes[0].get_deterministic_priv_key().address)

        self.sync_all()

        gbtl0 = self.nodes[0].getblocktemplatelight()
        gbtl1 = self.nodes[1].getblocktemplatelight()

        assert_blocktemplate_equal(gbtl0, gbtl1)

        # some random tx's from mainnet and testnet. They don't have to be valid on this chain for this test.
        txs = [
            "01000000016af14fe2b65b3fabe6a8f125de5ded1180aff9c3892138eefc16242f2dadfe2f00000000fd8a0100483045022100d80"
            "fa2758e4c1bc2b5b687b59d853c3a97e2b343b9ae1cb2bea0dce0e2cb1ca602200ac71e79dcde5d065ac99160be3376c8a373c016"
            "b5b6cef584b9a8eeb901b0a841483045022100d6a1a7393fa728404790bc85c26b60cf4d6d2baecfefca8b01608bb02441dc7c022"
            "056922cc8fa4d14eed39a69287a89c9d630164c23f4f810fa774e3feb6cdfea584147304402203f6a7ab7a5b91b0495ff6be292a5"
            "eee74bbf5c7b1cc6de586002ccf4142059a302200cf80778d4f4c078073d840b027a927a11d227bb87cbd043c37989f5cb01861c4"
            "14cad532102962feabd55f69c0e8eaceb7df43969dda4aeb575c7a501a4d08be392b2c48f2a2102a0e6e0def65cdb686a85c9a5cc"
            "03fc4c524831806f03cc7a18463b5267a5961421030b61fc10e70ca4fcedf95ca8284b545a5a80f255205c1c19e5eebcadbc17365"
            "921036d623ebfc46b97eb99a43d3c45df09319c8a6c9ba2b292c1a6a42e460034ed7a2103f54a07c2b5e82cf1e6465d7e37ee5a4b"
            "0701b2ccda866430190a8ebbd00f07db55aefeffffff022c1172000000000017a914e78564d75c446f8c00c757a2bd783d30c4f08"
            "19a8740e88e02000000001976a91471faafd5016aa8255d61e95cfe3c4f180504051e88ac48a80900",

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

        node = self.nodes[0]
        gbt_dir = os.path.join(node.datadir, self.chain, 'gbt')
        trash_dir = os.path.join(gbt_dir, 'trash')

        def path_for_job(jid): return os.path.join(gbt_dir, jid)

        def trash_path_for_job(jid): return os.path.join(trash_dir, jid)

        self.log.info("gbt_dir: {}".format(gbt_dir))
        self.log.info("trash_dir: {}".format(trash_dir))

        gbtl = []
        job_ids = set()
        trashed_ids = set()
        removed_ids = set()
        stop_flag = threading.Event()

        def check_jobs():
            for j in set(job_ids):
                if not os.path.exists(path_for_job(j)):
                    if os.path.exists(trash_path_for_job(j)):
                        if not j in trashed_ids:
                            self.log.info(f'found trashed job {j}')
                            trashed_ids.add(j)
                    else:
                        if not j in removed_ids:
                            self.log.info(f'found removed job {j}')
                            removed_ids.add(j)

        def poll_thread():
            """This thread is necessary to scan the gbt_dir and trash_dir and not miss any files.
            It is a workaround to very slow gitlab CI (especially on aarch64)."""
            while not stop_flag.wait(0.100):  # poll every 100ms
                check_jobs()

        pthr = threading.Thread(target=poll_thread, daemon=True)
        pthr.start()

        try:
            # generate a bunch of unique job_ids
            txs_tmp = txs
            n_iters = self._cache_size * 3  # intentionally overfill past cache size
            assert n_iters
            for _ in range(n_iters):
                tstart = time.time()
                gbtl_res = node.getblocktemplatelight({}, txs_tmp)
                telapsed = time.time() - tstart
                jid = gbtl_res['job_id']
                self.log.info(f'getblocktemplatelight returned job {jid} in {telapsed:.2f} seconds')
                job_ids.add(jid)
                gbtl.append(gbtl_res)
                txs_tmp += txs
        finally:
            # Ensure subordinate poller thread is stopped, joined
            stop_flag.set()
            pthr.join()

        assert os.path.isdir(gbt_dir)
        assert os.path.isdir(trash_dir)
        assert len(job_ids) == n_iters

        def predicate():
            check_jobs()
            return job_ids == removed_ids

        wait_until(predicate, timeout=self._store_time * 2)

        assert_equal(job_ids, removed_ids)
        assert_equal(0, len(os.listdir(trash_dir)))

        # grab ids for jobs that are no longer in the in-memory LRU cache  -- they should all raise now that their
        # job data was deleted from disk.
        job_ids = [x['job_id'] for x in gbtl[:-self._cache_size]]

        assert job_ids and len(job_ids) == (n_iters - self._cache_size)

        # now, test that all the deleted ones are truly gone and raise the proper RPC error
        for i, job_id in enumerate(job_ids):
            tmpl = gbtl[i]
            block = messages.CBlock()
            block.nVersion = tmpl["version"]
            block.hashPrevBlock = int(tmpl["previousblockhash"], 16)
            block.nTime = tmpl["curtime"]
            block.nBits = int(tmpl["bits"], 16)
            block.nNonce = 0
            coinbase_tx = blocktools.create_coinbase(height=int(tmpl["height"]) + 1)
            coinbase_tx.vin[0].nSequence = 2 ** 32 - 2
            coinbase_tx.rehash()
            block.vtx = [coinbase_tx]
            assert_raises_rpc_error(-8,
                                    "job_id data not available",
                                    node.submitblocklight,
                                    block.serialize().hex(), job_id)


if __name__ == '__main__':
    GBTLightBGCleanerTest().main()
