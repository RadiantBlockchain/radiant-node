#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
""" Test for the DoubleSpend Proof RPCs """

import time
from copy import deepcopy
from decimal import Decimal
from io import BytesIO
from test_framework.blocktools import create_raw_transaction
from test_framework.messages import CDSProof, msg_dsproof
from test_framework.p2p import P2PInterface, p2p_lock
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_greater_than, find_output, wait_until
)


class DoubleSpendProofRPCTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def basic_check(self):
        """Tests basic getdsproof/getdsprooflist functionality"""

        self.generate(self.nodes[0], 1)
        self.sync_all()

        # Create and mine a regular non-coinbase transaction for spending
        funding_txid = self.nodes[0].getblock(self.nodes[0].getblockhash(1))['tx'][0]

        # Create three conflicting transactions. They are only signed, but not yet submitted to the mempool
        first_ds_tx = create_raw_transaction(self.nodes[0], funding_txid, self.nodes[0].getnewaddress(), 49.95)
        second_ds_tx = create_raw_transaction(self.nodes[0], funding_txid, self.nodes[0].getnewaddress(), 49.95)

        first_ds_tx_id = self.nodes[0].sendrawtransaction(first_ds_tx)
        assert_equal(self.nodes[0].getdsproofscore(first_ds_tx_id), 1.0)  # score is 1.0 until we see a dsproof
        second_ds_tx_id = self.nodes[1].call_rpc('sendrawtransaction', second_ds_tx,
                                                 ignore_error='txn-mempool-conflict')

        vout = find_output(self.nodes[0], first_ds_tx_id, Decimal('49.95'))
        child = create_raw_transaction(self.nodes[0], first_ds_tx_id, self.nodes[0].getnewaddress(), 49.90, vout)
        child_id = self.nodes[0].sendrawtransaction(child)

        vout = find_output(self.nodes[0], child_id, Decimal('49.90'))
        grandchild = create_raw_transaction(self.nodes[0], child_id, self.nodes[0].getnewaddress(), 49.85, vout)
        grandchild_id = self.nodes[0].sendrawtransaction(grandchild)
        # Wait until both nodes see the same dsproof
        wait_until(
            lambda: len(self.nodes[0].getdsprooflist()) == 1 and len(self.nodes[1].getdsprooflist()) == 1,
            timeout=10
        )

        assert_equal(self.nodes[0].getdsproofscore(first_ds_tx_id), 0.0)  # score is 0 after we see a dsproof

        dsplist = self.nodes[0].getdsprooflist()
        assert_equal(len(dsplist), 1)
        assert isinstance(dsplist[0], str)
        assert isinstance(self.nodes[0].getdsprooflist(1)[0], dict)

        # Get a DSP by DspId
        dsp = self.nodes[0].getdsproof(dsplist[0])
        dsp_node1 = self.nodes[1].getdsproof(dsplist[0])
        # each node has the same dsproof, but associated with the txid it sees in mempool
        assert_equal(dsp["txid"], first_ds_tx_id)
        if second_ds_tx_id is not None:
            # common case where node1 saw the RPC tx2 before the p2p tx1
            assert_equal(dsp_node1["txid"], second_ds_tx_id)
        else:
            # node1 happened to see the first tx via p2p first
            assert_equal(dsp_node1["txid"], first_ds_tx_id)
        # we expect this dsp tx to invalidate 3 tx's total (parent, child, and grandchild)
        assert_equal(len(dsp["descendants"]), 3)
        # Check that the dsp has the "outpoint" key and it is what we expect
        ds_outpoint = {
            "txid": funding_txid,
            "vout": 0
        }
        assert_equal(dsp["outpoint"], ds_outpoint)

        # Get a DSP by double spending txid
        assert "txid" in self.nodes[0].getdsproof(first_ds_tx_id)

        # Get a DSP by txid of a transaction in a double spent chain
        assert_equal(len(self.nodes[0].getdsproof(child_id)["path"]), 2)
        assert_equal(len(self.nodes[0].getdsproof(grandchild_id)["path"]), 3)

        # A non-recursive call for the double spent transaction will result in a DSP
        assert self.nodes[0].getdsproof(first_ds_tx_id, 0, False) is not None
        # A non-recursive call for a child transaction in the double spent chain
        # will not result in a DSP
        assert self.nodes[0].getdsproof(child_id, 0, False) is None

        # Check that the list and the get calls outputs match for the same verbosity level,
        # and that they contain the keys we expect for each verbosity level
        verb_keys = {
            1: {"txid", "hex"},
            2: {"dspid", "txid", "outpoint"},
            3: {"dspid", "txid", "outpoint", "spenders"},
        }
        spenders_keys = {"txversion", "sequence", "locktime", "hashprevoutputs", "hashsequence", "hashoutputs",
                         "pushdata"}
        for verbosity in (1, 2, 3):
            dspid = dsplist[0]
            dsp = self.nodes[0].getdsproof(dspid, verbosity)
            dsp.pop("descendants", None)  # Remove key that is missing from the list mode
            dlist = self.nodes[0].getdsprooflist(verbosity)
            for dsp2 in dlist:
                if dsp == dsp2:
                    break
            else:
                assert False, "Could not find the dsp we expected in the dsplist"
            assert_equal(dsp["txid"], first_ds_tx_id)  # ensure txid always what we expect
            expected_keys = verb_keys[verbosity]
            assert_equal(expected_keys & set(dsp.keys()), expected_keys)
            if "dspid" in expected_keys:
                assert_equal(dsp["dspid"], dspid)
            if "outpoint" in expected_keys:
                assert_equal(dsp["outpoint"], ds_outpoint)
            if "spenders" in expected_keys:
                for spender in dsp["spenders"]:
                    assert_equal(spenders_keys & set(spender.keys()), spenders_keys)
            if "hex" in expected_keys:
                # ensure that the dsproof hex data decodes ok
                data = bytes.fromhex(dsp["hex"])
                # dsproof serialized data cannot be smaller than 216 bytes
                assert_greater_than(len(data), 216)

        # If any of the competing transactions is mined, the DPSs are put in the orphan list
        self.generate(self.nodes[0], 1)
        self.sync_all()
        assert_equal(len(self.nodes[0].getdsprooflist()), 0)  # no non-orphan results
        dsps_all_orphans = self.nodes[0].getdsprooflist(0, True)
        assert_equal(dsps_all_orphans, dsplist)  # all of the previous proofs are orphans
        for dspid in dsps_all_orphans:
            # make sure they are all orphans by checking they have no 'txid' key
            assert self.nodes[0].getdsproof(dspid).get('txid') is None

    def paths_check(self):
        """Check that:
         - the 'paths' key in a lookup by child txid works as expected,
         - the 'descendants' key in the lookup of a dsp works as expected. """

        self.generate(self.nodes[0], 1)
        self.sync_all()

        # Create a transaction on a double spent chain with multiple paths back to the DSP
        # The following scenario of txos will occur:
        # A -> B -> C -> D -> E
        #  \           /
        #   -> Z ---->/
        # Where there is a known DSP for A
        # and D contains inputs from C and Z
        # exactly one path from D to A will be found
        # (either [Z->D, A->Z] or [C->D, B->C, A->B])

        paths = [[], []]

        # Here we spend from A to B and Z
        funding_txid = self.nodes[0].getblock(self.nodes[0].getblockhash(2))['tx'][0]
        inputs = [{"txid": funding_txid, "vout": 0}]
        b_address = self.nodes[0].getnewaddress()
        z_address = self.nodes[0].getnewaddress()
        outputs = {b_address: 24.99, z_address: 24.99}
        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        signresult = self.nodes[0].signrawtransactionwithwallet(rawtx)
        assert_equal(signresult["complete"], True)
        transaction_2_outputs = signresult['hex']

        doublespendingtransaction = create_raw_transaction(self.nodes[0], funding_txid,
                                                           self.nodes[0].getnewaddress(), 49.97, 0)

        transaction_2outputs_id = self.nodes[0].sendrawtransaction(transaction_2_outputs)
        self.nodes[1].call_rpc('sendrawtransaction', doublespendingtransaction, ignore_error='txn-mempool-conflict')
        paths[0].insert(0, transaction_2outputs_id)  # root of both possible paths
        paths[1].insert(0, transaction_2outputs_id)

        # Here we spend from B to C
        child = create_raw_transaction(self.nodes[0], transaction_2outputs_id,
                                       self.nodes[0].getnewaddress(), 24.96, 0)
        child_id = self.nodes[0].sendrawtransaction(child)
        paths[1].insert(0, child_id)  # exists only on longer path

        d_address = self.nodes[0].getnewaddress()
        vout = find_output(self.nodes[0], child_id, Decimal('24.96'))
        # Here we spend from Z and C to D
        inputs = [{"txid": transaction_2outputs_id, "vout": 1}, {"txid": child_id, "vout": vout}]
        outputs = {d_address: 49.94}
        rawtx = self.nodes[0].createrawtransaction(inputs, outputs)
        signresult = self.nodes[0].signrawtransactionwithwallet(rawtx)
        assert_equal(signresult["complete"], True)
        transaction_2inputs = signresult['hex']
        transaction_2inputs_id = self.nodes[0].sendrawtransaction(transaction_2inputs)
        paths[0].insert(0, transaction_2inputs_id)  # exists of both possible paths
        paths[1].insert(0, transaction_2inputs_id)

        # add 1 more grandchild tx for good measure
        e_tx = create_raw_transaction(self.nodes[0], transaction_2inputs_id, self.nodes[0].getnewaddress(), 24.95 * 2, 0)
        e_txid = self.nodes[0].sendrawtransaction(e_tx)
        paths_shorter = deepcopy(paths)
        paths[0].insert(0, e_txid)  # leaf tx of both possible paths
        paths[1].insert(0, e_txid)

        wait_until(
            lambda: len(self.nodes[0].getdsprooflist()) == 1,
            timeout=10
        )
        dsplist = self.nodes[0].getdsprooflist()
        self.log.info(f"dsplist: {dsplist}")
        dsp = self.nodes[0].getdsproof(dsplist[0])
        descendants_expected = {txid for txid in paths[0] + paths[1]}
        self.log.info(f"dsp: {dsp} descendants_expected: {descendants_expected}")
        assert_equal(set(dsp["descendants"]), descendants_expected)

        dsp = self.nodes[0].getdsproof(e_txid)
        self.log.info(f"dsp: {dsp}, paths: {paths}")
        assert dsp is not None
        assert "path" in dsp
        # make sure the path from query txid to double-spend txid is one of the two possible paths
        assert dsp["path"] in paths
        self.log.info(f"dsp: {dsp}")
        self.log.info(f"dsp path len: {len(dsp['path'])}")
        assert_equal(dsp["txid"], transaction_2outputs_id)

        # now go up one, query by previous txid, we should get a shorter path
        dsp2 = self.nodes[0].getdsproof(transaction_2inputs_id)
        assert dsp2 != dsp
        assert dsp2["path"] in paths_shorter
        assert_equal(dsp2["descendants"], dsp["descendants"])
        assert_equal(dsp2["dspid"], dsp["dspid"])
        assert_equal(dsp2["txid"], dsp["txid"])

    def orphan_list_check(self):
        """Test that:
         - getdsprooflist omits orphans and/or includes them as expected for the include_orphans arg
         - the orphans appearing in the orphan list match what we expect"""

        # assumption is previous test left some proofs around
        len_noorphans = len(self.nodes[0].getdsprooflist(False, False))
        assert_greater_than(len_noorphans, 0)
        # previous test may or may not have left some orphans around, account for them
        len_orphans = len(self.nodes[0].getdsprooflist(False, True)) - len_noorphans

        orphans_ids = [
            "978c2b3d829dbc934c170ff797c539a86d35fcfc0ec806a5753c00794cd5caad",
            "0e7f2e002073916cfa16692df9b44c2d52e808f7b55e75e7d941356dd90f2096",
        ]
        orphans_data = [bytes.fromhex(hx) for hx in (
            "326bd6eee699d18308a04720583f663ba039070a1abd8a59868ee03a1c250be10000000001000000feffffffbadb1500dbd7cf882"
            "c620ed8fb660b64444bfb4febf6c553d0f19cafdc3070bc2c27664618606b350cd8bf565266bc352f0caddcf01e8fa789dd8a1538"
            "6327cf8cabe198fdef7e5d2f370d4e96ab7cc22482f181b2c0e7e6275838aeed19eeedbfd378170141467adbad7deb7635bdf6bbe"
            "4c605ce57c3dccd01f5fb9e32b22c3479a1d3f143f3f9592f9e1ef9ea96f01141b261e468c46d31a4a63cde692947d126f34641e3"
            "4101000000feffffffbadb1500dbd7cf882c620ed8fb660b64444bfb4febf6c553d0f19cafdc3070bc2c27664618606b350cd8bf5"
            "65266bc352f0caddcf01e8fa789dd8a15386327cf8cabe198188af582e7a09fcfe0b1a37ee3ca6c91f80c13006e595c79320ac38d"
            "40a945cf0141210f8a36fe24b9fb1cb5a2a8cb01ac27d58410d8d8f3abf6fe935b2b1c1eadb285a4cdcd24727472af4d65b1c7ccb"
            "120361bdcbcadfb2f1436df9bfe9b9a5b0641",
            "11565d6e11586e4d0b989358be23299458afd76d8eedad32f96a671f775970740000000001000000feffffff4fdc1500ac7850e9b"
            "64559f703a9e6068bde8c175761408f0777299691083b0fc534aef618606b350cd8bf565266bc352f0caddcf01e8fa789dd8a1538"
            "6327cf8cabe198cdf604b8294fe87f39c637bcab10869db7cc306b0ddbf35ed92ab526dd18af69014126b3e82473f456f9bcb2bf1"
            "20ede1ad6e3ee588935b70033cfb625c317ced26f116b54d2effc3c9abf5efd38cffae57af50fb5fef88e1be7dc9d82a415fc1367"
            "4101000000feffffff4fdc1500ac7850e9b64559f703a9e6068bde8c175761408f0777299691083b0fc534aef618606b350cd8bf5"
            "65266bc352f0caddcf01e8fa789dd8a15386327cf8cabe1981194289492779938f938ce59ba48f916ba0b883803fbf2bfab22bf8d"
            "b09227ba0141fdb9ff69c028a6a1e4143bedcf2f44b1ea2b6996bd463440f9d3037845ad7a879963acbb424d3850ba6affdf81325"
            "e7753294a2e1959d9e84ba6108ce15e7cdc41",
        )]
        orphans_proofs = []
        orphans_outpoints = set()
        for od in orphans_data:
            proof = CDSProof()
            proof.deserialize(BytesIO(od))
            orphans_outpoints.add((
                int.to_bytes(proof.prevTxId, 32, byteorder='big').hex(),  # txid
                proof.prevOutIndex  # vout
            ))
            orphans_proofs.append(proof)
        assert len(orphans_outpoints) == 2

        p2p = P2PInterface()
        self.nodes[0].add_p2p_connection(p2p)
        wait_until(
            lambda: sum(p2p.message_count.values()) > 0,
            lock=p2p_lock
        )

        # send orphans to node0
        for proof in orphans_proofs:
            p2p.send_message(msg_dsproof(proof))

        # wait for node0 to have acknowledged the orphans
        wait_until(
            lambda: len(self.nodes[0].getdsprooflist(False, True)) == len_noorphans + len_orphans + 2
        )

        def check(len_noorphans, len_orphans):
            # verify that node0 has a view of the orphans that we expect
            dsplist_all = self.nodes[0].getdsprooflist(False, True)
            non_orph_ct = 0
            orph_ct = 0
            orphs_seen = set()
            outpoints_seen = set()
            matches = 0
            for dspid in dsplist_all:
                dsp = self.nodes[0].getdsproof(dspid, True)
                if dsp["txid"] is not None:
                    non_orph_ct += 1
                    continue
                orphs_seen.add(dsp["dspid"])
                orph_ct += 1
                op = dsp["outpoint"]
                outpoints_seen.add((op["txid"], op["vout"]))
                hexdata = self.nodes[0].getdsproof(dspid, False)["hex"]
                try:
                    # ensure hexdata we got for this dspid matches our data
                    orphans_data.index(bytes.fromhex(hexdata))
                    orphans_ids.index(dspid)
                    matches += 1
                except ValueError:
                    pass  # this is ok, stale oprhan (ignore)
            assert_equal(matches, len(orphans_data))
            assert_equal(non_orph_ct, len_noorphans)
            assert_equal(orph_ct, len_orphans)
            assert_equal(orphs_seen & set(orphans_ids), set(orphans_ids))
            assert_equal(outpoints_seen & orphans_outpoints, orphans_outpoints)

        check(len_noorphans=len_noorphans, len_orphans=len_orphans + len(orphans_ids))

        # Mining a block should take every dsproof and send them to the orphan list
        # it should also keep the same orphans from before around
        block_hash = self.generate(self.nodes[0], 1)[0]
        self.sync_all()
        # No non-orphans
        assert_equal(len(self.nodes[0].getdsprooflist()), 0)
        # All previous dsproofs are now orphans (because their associated tx was mined
        assert_equal(len(self.nodes[0].getdsprooflist(False, True)), len_orphans + len_noorphans + len(orphans_ids))

        # Test reorg behavior. On reorg, all tx's that go back to mempool should continue to have their previous
        # proofs. We invalidate the block and make sure that all the orphaned dsp's got claimed again by their
        # respective tx's which were put back into the mempool
        self.nodes[0].invalidateblock(block_hash)
        wait_until(
            lambda: len(self.nodes[0].getdsprooflist()) == len_noorphans,
            timeout=10
        )
        check(len_noorphans=len_noorphans, len_orphans=len_orphans + len(orphans_ids))
        # Now put the block back
        self.nodes[0].reconsiderblock(block_hash)
        self.sync_all()
        # There should again be no non-orphans
        assert_equal(len(self.nodes[0].getdsprooflist()), 0)
        # All previous dsproofs are now orphans again
        assert_equal(len(self.nodes[0].getdsprooflist(False, True)), len_orphans + len_noorphans + len(orphans_ids))

        # Wait for all orphans to get auto-cleaned (this may take up to 60 seconds)
        self.nodes[0].setmocktime(int(time.time() + 100))
        wait_until(
            lambda: len(self.nodes[0].getdsprooflist(False, True)) == 0,
            timeout=90
        )

    def p2sh_score_check(self):
        """Create a P2SH, send to the P2SH, create a child of it that sends to P2PKH, and check dsproof scores"""
        self.generate(self.nodes[0], 1)
        self.sync_all()
        pubkey = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())['pubkey']
        p2sh = self.nodes[0].addmultisigaddress(1, [pubkey], "")['address']
        to_p2sh_txid = self.nodes[0].sendtoaddress(p2sh, 49)
        vout = find_output(self.nodes[0], to_p2sh_txid, Decimal('49'))
        self.sync_all()
        # Now spend it to a P2PKH
        from_p2sh_tx = create_raw_transaction(self.nodes[0], to_p2sh_txid, self.nodes[0].getnewaddress(), 48.9, vout)
        from_p2sh_txid = self.nodes[0].sendrawtransaction(from_p2sh_tx)
        # Test that getdsproofscore should be 1.0 for a tx that sends to p2sh but itself spends p2pkh
        assert_equal(self.nodes[0].getdsproofscore(to_p2sh_txid), 1.0)
        # Test that getdsproofscore should be 0.0 for a tx that spends from a p2sh to a p2pkh
        assert_equal(self.nodes[0].getdsproofscore(from_p2sh_txid), 0.0)
        # Now mine a block to confirm just the above 2 txns
        prev_blockhash = self.nodes[0].getblockchaininfo()["bestblockhash"]
        blockhash, = self.generate(self.nodes[0], 1)
        assert_equal(self.nodes[0].getrawmempool(False), [])
        assert_equal(self.nodes[0].getblockchaininfo()["bestblockhash"], blockhash)
        # Next invalidate the above block, so that we send the parent txns back to mempool
        self.nodes[0].invalidateblock(blockhash)
        wait_until(
            lambda: self.nodes[0].getblockchaininfo()["bestblockhash"] == prev_blockhash,
            timeout=30
        )
        # Check that txns were resurrected and put back into the mempool
        assert_equal(sorted(self.nodes[0].getrawmempool(False)), sorted([to_p2sh_txid, from_p2sh_txid]))
        # Test that the children of the 0.0 tx, which all spend a p2pkh, inherit the 0.0 score
        prev_txid = from_p2sh_txid
        prev_amount = Decimal('48.9')
        txids = []
        for _ in range(10):
            vout = find_output(self.nodes[0], prev_txid, prev_amount)
            amount = prev_amount - Decimal('0.1')
            child_tx = create_raw_transaction(self.nodes[0], prev_txid, self.nodes[0].getnewaddress(), amount, vout)
            child_txid = self.nodes[0].sendrawtransaction(child_tx)
            assert_equal(self.nodes[0].getdsproofscore(child_txid), 0.0)
            prev_txid = child_txid
            prev_amount = amount
            txids.append(child_txid)
        # Now, bring back the block, confirming the parents that "contaminated" the children
        # with the 0.0 score. The scores now for all children should 1.0.
        self.nodes[0].reconsiderblock(blockhash)
        wait_until(
            lambda: self.nodes[0].getblockchaininfo()["bestblockhash"] == blockhash,
            timeout=30
        )
        assert_equal(sorted(self.nodes[0].getrawmempool(False)), sorted(txids))
        # The scores are 1.0 for all the child txns after their score=0.0 parents are confirmed
        assert all(self.nodes[0].getdsproofscore(txid) == 1.0 for txid in txids)

    def run_test(self):
        self.basic_check()
        self.paths_check()
        self.orphan_list_check()
        self.p2sh_score_check()


if __name__ == '__main__':
    DoubleSpendProofRPCTest().main()
