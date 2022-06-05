#!/usr/bin/env python3
# Copyright (c) 2021 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""
This tests the new May 2022 upgrade8 feature: 64-bit script integers
as well as the re-enabled OP_MUL opcode.
"""

from typing import Tuple

from test_framework.address import (
    script_to_p2sh, hash160
)
from test_framework.blocktools import (
    create_block,
    create_coinbase,
    create_tx_with_script,
    make_conform_to_ctor,
)
from test_framework.key import ECKey
from test_framework.messages import (
    CBlock,
    COutPoint,
    CTransaction,
    CTxIn,
    CTxOut,
    FromHex,
)
from test_framework.p2p import (
    P2PDataStore,
)
from test_framework import schnorr
from test_framework.script import (
    CScript,
    CScriptNum,
    OP_1NEGATE, OP_0, OP_1, OP_2, OP_3, OP_4, OP_6, OP_7, OP_8, OP_10, OP_15, OP_16,
    OP_CHECKMULTISIG, OP_EQUALVERIFY, OP_HASH160, OP_EQUAL, OP_NUM2BIN, OP_BIN2NUM, OP_PICK,
    OP_ADD,
    OP_DIV,
    OP_MOD,
    OP_MUL,
    OP_SUB,
    OP_TRUE,
    OP_DROP,
    SIGHASH_ALL,
    SIGHASH_FORKID,
    SignatureHashForkId,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import assert_equal

# Overflow failure
OVERFLOW_ERROR_BAD_OPERAND = ('mandatory-script-verify-flag-failed '
                              '(Given operand is not a number within the valid range [-2^63 + 1, 2^63 - 1])')
# Overflow if one of the operands coming in from a push off the stack is out-of-range (known issue with interpreter)
OVERFLOW_ERROR_UNK = 'mandatory-script-verify-flag-failed (unknown error)'
# OP_NUM2BIN failure when we try to encode a number that won't fit within the requested size
IMPOSSIBLE_ENCODING_ERROR = 'mandatory-script-verify-flag-failed (The requested encoding is impossible to satisfy)'
# OP_PICK if the index is out of bounds
INVALID_STACK_OPERATION = 'mandatory-script-verify-flag-failed (Operation not valid with the current stack size)'


class Int64CScriptNum(BitcoinTestFramework):

    def set_test_params(self):
        self.num_nodes = 1
        self.block_heights = {}
        self.extra_args = [
            # Node0 has bigint64 activated (activates at upgrade8)
            ["-acceptnonstdtxn=1", "-expire=0"],
        ]

    def bootstrap_p2p(self, *, num_connections=1):
        """Add a P2P connection to the node.

        Helper to connect and wait for version handshake."""
        for _ in range(num_connections):
            self.nodes[0].add_p2p_connection(P2PDataStore())
        for p2p in self.nodes[0].p2ps:
            p2p.wait_for_getheaders()

    def reconnect_p2p(self, **kwargs):
        """Tear down and bootstrap the P2P connection to the node.

        The node gets disconnected several times in this test. This helper
        method reconnects the p2p and restarts the network thread."""
        self.nodes[0].disconnect_p2ps()
        self.bootstrap_p2p(**kwargs)

    def getbestblock(self, node):
        """Get the best block. Register its height so we can use build_block."""
        block_height = node.getblockcount()
        blockhash = node.getblockhash(block_height)
        block = FromHex(CBlock(), node.getblock(blockhash, 0))
        block.calc_sha256()
        self.block_heights[block.sha256] = block_height
        return block

    def build_block(self, parent, transactions=(), nTime=None):
        """Make a new block with an OP_1 coinbase output.

        Requires parent to have its height registered."""
        parent.calc_sha256()
        block_height = self.block_heights[parent.sha256] + 1
        block_time = (parent.nTime + 1) if nTime is None else nTime

        block = create_block(
            parent.sha256, create_coinbase(block_height), block_time)
        block.vtx.extend(transactions)
        make_conform_to_ctor(block)
        block.hashMerkleRoot = block.calc_merkle_root()
        block.solve()
        self.block_heights[block.sha256] = block_height
        return block

    def run_test(self):
        node = self.nodes[0]

        self.bootstrap_p2p()

        self.log.info("Create some blocks with OP_1 coinbase for spending.")
        tip = self.getbestblock(node)
        blocks = []
        for _ in range(10):
            tip = self.build_block(tip)
            blocks.append(tip)
        node.p2p.send_blocks_and_test(blocks, node, success=True)
        spendable_txns = [block.vtx[0] for block in blocks]

        self.log.info("Mature the blocks and get out of IBD.")
        self.generatetoaddress(node, 100, node.get_deterministic_priv_key().address)

        self.log.info("Setting up spends to test and mining the fundings")

        # Generate a key pair
        privkeybytes = b"INT64!!!" * 4
        private_key = ECKey()
        private_key.set(privkeybytes, True)
        # get uncompressed public key serialization
        public_key = private_key.get_pubkey().get_bytes()

        def create_fund_and_spend_tx(scriptsigextra, redeemextra) -> Tuple[CTransaction, CTransaction]:
            spendfrom = spendable_txns.pop()

            redeem_script = CScript(redeemextra + [OP_1, public_key, OP_1, OP_CHECKMULTISIG])
            script_pubkey = CScript([OP_HASH160, hash160(redeem_script), OP_EQUAL])

            value = spendfrom.vout[0].nValue
            value1 = value - 500

            # Fund transaction
            txfund = create_tx_with_script(spendfrom, 0, b'', value1, script_pubkey)
            txfund.rehash()

            p2sh = script_to_p2sh(redeem_script)
            self.log.info(f"scriptPubKey {script_pubkey!r}")
            self.log.info(f"redeemScript {redeem_script!r} -> p2sh address {p2sh}")

            # Spend transaction
            value2 = value1 - 500
            txspend = CTransaction()
            txspend.vout.append(
                CTxOut(value2, CScript([OP_TRUE])))
            txspend.vin.append(
                CTxIn(COutPoint(txfund.sha256, 0), b''))

            # Sign the transaction
            sighashtype = SIGHASH_ALL | SIGHASH_FORKID
            hashbyte = bytes([sighashtype & 0xff])
            sighash = SignatureHashForkId(
                redeem_script, txspend, 0, sighashtype, value1)
            txsig = schnorr.sign(privkeybytes, sighash) + hashbyte
            dummy = OP_1  # Required for 1-of-1 schnorr sig
            txspend.vin[0].scriptSig = ss = CScript([dummy, txsig] + scriptsigextra + [redeem_script])
            self.log.info(f"scriptSig: {ss!r}")
            txspend.rehash()

            return txfund, txspend

        mempool = []

        # Basic test of OP_MUL 2 * 3 = 6
        tx0, tx = create_fund_and_spend_tx([OP_2, OP_3], [OP_MUL, OP_6, OP_EQUALVERIFY])
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle the output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Basic test of OP_DIV 6 / 3 = 2
        tx0, tx = create_fund_and_spend_tx([OP_6, OP_3], [OP_DIV, OP_2, OP_EQUALVERIFY])
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide 2^63-1 by 1 -- This should be 100% ok
        ssextra = [CScriptNum(int(2**63 - 1)), OP_1]
        rsextra = [OP_DIV, CScriptNum(int(2**63 - 1) // 1), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide -2^63-1 / -2^63-1 -- This should be 100% ok
        ssextra = [CScriptNum(-int(2**63 - 1)), CScriptNum(-int(2**63 - 1))]
        rsextra = [OP_DIV, OP_1, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide -2^63-1 / 2^63-1 -- This should be 100% ok
        ssextra = [CScriptNum(-int(2**63 - 1)), CScriptNum(int(2**63 - 1))]
        rsextra = [OP_DIV, OP_1NEGATE, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide 2^63-1 / -2^63-1 -- This should be 100% ok
        ssextra = [CScriptNum(int(2**63 - 1)), CScriptNum(-int(2**63 - 1))]
        rsextra = [OP_DIV, OP_1NEGATE, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide 2^63-1 / 2^63-1 -- This should be 100% ok
        ssextra = [CScriptNum(int(2**63 - 1)), CScriptNum(int(2**63 - 1))]
        rsextra = [OP_DIV, OP_1, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Multiply past 2^32 -- should work
        ssextra = [CScriptNum(int(2**31)), CScriptNum(int(2**31))]
        rsextra = [OP_MUL, CScriptNum(int(2**62)), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Add past (2^31 - 1) -- should work
        ssextra = [CScriptNum(int(2**31)), CScriptNum(int(2**31))]
        rsextra = [OP_ADD, CScriptNum(int(2**32)), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Sub below -(2^31 - 1) -- should work
        ssextra = [CScriptNum(-int(2**31 - 1)), CScriptNum(int(2**31 - 1))]
        rsextra = [OP_SUB, CScriptNum(-int(2**32 - 2)), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide/multiply mixed: -2^60 * 3 / 6 == -2^59
        ssextra = [CScriptNum(-int(2**60)), OP_3]
        rsextra = [OP_MUL, OP_6, OP_DIV, CScriptNum(-int(2**59)), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide: -2^31 * 3 / -6 == 2^30 (intermediate value outside of 32-bit range)
        ssextra = [CScriptNum(-int(2**31)), OP_3]
        rsextra = [OP_MUL, CScriptNum(-6), OP_DIV, CScriptNum(int(2**30)), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Divide: -2^32 * 3 / -6 == 2^31 (1 operand & intermediate value > 2^31 - 1)
        ssextra = [CScriptNum(-int(2**32)), OP_3]
        rsextra = [OP_MUL, CScriptNum(-6), OP_DIV, CScriptNum(int(2**31)), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Multiply past 2^63 - 1 -- funding tx is ok, spending should not be accepted due to out-of-range operand
        ssextra = [CScriptNum(int((2**63) - 1)), OP_3]
        rsextra = [OP_MUL, OP_DROP, OP_1, OP_1, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=OVERFLOW_ERROR_BAD_OPERAND)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # Add past 2^63 - 1 -- funding tx is ok, spending should not be accepted due to bad operand
        ssextra = [CScriptNum(int((2**63) - 1)), OP_1]
        rsextra = [OP_ADD, OP_DROP, OP_1, OP_1, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=OVERFLOW_ERROR_BAD_OPERAND)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # Sub below -2^63 - 1 -- funding tx is ok, spending should not be accepted due to bad operand
        ssextra = [CScriptNum(-int((2**63) - 1)), OP_10]
        rsextra = [OP_SUB, OP_DROP, OP_1, OP_1, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=OVERFLOW_ERROR_BAD_OPERAND)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # Modulo: -(2^63 - 1) % -1. Should not overflow, but yield 0
        ssextra = [CScriptNum(-int((2**63) - 1)), OP_1NEGATE]
        rsextra = [OP_MOD, OP_0, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Modulo: -(2^63 - 1) % -(2^63 - 1). Should not overflow, but yield 0
        ssextra = [CScriptNum(-int((2**63) - 1)), CScriptNum(-int((2**63) - 1))]
        rsextra = [OP_MOD, OP_0, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Attempt to create the forbidden: -(2^63), but don't use it as a number
        ssextra = [bytes((0x80,)) + bytes((0x7f,)) + bytes((0xff,) * 7)]
        rsextra = [bytes((0x80,)) + bytes((0x7f,)) + bytes((0xff,) * 7), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # Attempt to create the forbidden: -(2^63), use it as a number
        # Note: This generates an overflow exception when deserializing, hence the "unknown error" -- known issue
        #       with the interpreter.
        ssextra = [bytes((0x80,)) + bytes((0x7f,)) + bytes((0xff,) * 7)]
        rsextra = [bytes((0x80,)) + bytes((0x7f,)) + bytes((0xff,) * 7), OP_SUB, OP_0, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=OVERFLOW_ERROR_UNK)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # OP_NUM2BIN - {2^63 - 1} -> 8-byte BIN should succeed
        ssextra = [CScriptNum(int(2**63 - 1)), OP_8]
        rsextra = [OP_NUM2BIN, bytes.fromhex('ffffffffffffff7f'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_NUM2BIN - {2^63 - 1} -> 16-byte BIN should succeed
        ssextra = [CScriptNum(int(2**63 - 1)), OP_16]
        rsextra = [OP_NUM2BIN, bytes.fromhex('ffffffffffffff7f0000000000000000'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_NUM2BIN - {-2^63 + 1} -> 8-byte BIN should succeed
        ssextra = [CScriptNum(int(-2**63 + 1)), OP_8]
        rsextra = [OP_NUM2BIN, bytes.fromhex('ffffffffffffffff'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_NUM2BIN - {2^63 - 1} -> 7-byte BIN should fail because it won't fit within requested size
        ssextra = [CScriptNum(int(2**63 - 1)), OP_7]
        rsextra = [OP_NUM2BIN, OP_DROP]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=IMPOSSIBLE_ENCODING_ERROR)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # OP_NUM2BIN - {2^31 - 1} -> 8-byte BIN should succeed (check that old functionality still works)
        ssextra = [CScriptNum(int(2**31 - 1)), OP_8]
        rsextra = [OP_NUM2BIN, bytes.fromhex('ffffff7f00000000'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_NUM2BIN - {2^31 - 1} -> 4-byte BIN should succeed (check that old functionality still works)
        ssextra = [CScriptNum(int(2**31 - 1)), OP_4]
        rsextra = [OP_NUM2BIN, bytes.fromhex('ffffff7f'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_BIN2NUM - {BIN 2^63 - 1} should succeed
        ssextra = [CScriptNum(int(2**63 - 1))]
        rsextra = [OP_BIN2NUM, bytes.fromhex('ffffffffffffff7f'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_BIN2NUM - {BIN 2^63-1 padded with 8 extra bytes of zeroes} should succeed
        ssextra = [CScriptNum.encode(CScriptNum(int(2**63 - 1)))[1:] + bytes.fromhex('00') * 8]
        rsextra = [OP_BIN2NUM, bytes.fromhex('ffffffffffffff7f'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_BIN2NUM - {BIN -2^63+1} should succeed
        ssextra = [CScriptNum(int(-2**63 + 1))]
        rsextra = [OP_BIN2NUM, bytes.fromhex('ffffffffffffffff'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_BIN2NUM - {BIN 2^63} when crammed into 8 bytes will be treated as '-0'.. which, oddly, ends up as
        #              0 when using BIN2NUM. This is the same quirky behavior as before this feature was added,
        #              e.g.: 0x80 BIN2NUM -> 0
        ssextra = [bytes.fromhex('0000000000000080')]
        rsextra = [OP_BIN2NUM, OP_0, OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        assert_equal(node.getrawmempool(), mempool)

        # OP_BIN2NUM - {2^63} -> When encoding as not '-0', but what 2^63 would encode as (9-byte value), it should fail
        #                        because it's out of range.
        ssextra = [bytes.fromhex('000000000000008000')]
        rsextra = [OP_BIN2NUM, OP_DROP]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=OVERFLOW_ERROR_BAD_OPERAND)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # OP_BIN2NUM - {BIN 2^31-1} should succeed (check that old functionality still works)
        ssextra = [CScriptNum(int(2**31 - 1))]
        rsextra = [OP_BIN2NUM, bytes.fromhex('ffffff7f'), OP_EQUALVERIFY]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0, tx], node)
        mempool += [tx0.hash, tx.hash]
        spendable_txns.insert(0, tx)  # Recycle te output from this tx
        assert_equal(node.getrawmempool(), mempool)

        # OP_PICK - {4294967297 OP_PICK} should FAIL on both 64-bit and 32-bit systems
        ssextra = [OP_16, OP_15, CScriptNum(4294967297)]
        rsextra = [OP_PICK, OP_2, OP_PICK, OP_EQUALVERIFY, OP_DROP, OP_DROP]
        tx0, tx = create_fund_and_spend_tx(ssextra, rsextra)
        node.p2p.send_txs_and_test([tx0], node)
        node.p2p.send_txs_and_test([tx], node, success=False, expect_disconnect=True,
                                   reject_reason=INVALID_STACK_OPERATION)
        mempool += [tx0.hash]
        assert_equal(node.getrawmempool(), mempool)
        self.reconnect_p2p()  # we lost the connection from above bad tx, reconnect

        # Finally, mine the mempool and ensure that all txns made it into a block
        prevtiphash = node.getbestblockhash()
        tiphash = self.generatetoaddress(node, 1, node.get_deterministic_priv_key().address)[0]
        assert prevtiphash != tiphash
        assert_equal(node.getrawmempool(), [])
        blockinfo = node.getblock(tiphash, 1)
        assert all(txid in blockinfo['tx'] for txid in mempool)

        # --------------------------------------------------------------------
        # Test that scripts fail evaluation if bigint64 feature is disabled
        # --------------------------------------------------------------------

        # 1. Restart the node with -reindex-chainstate (to be paranoid)
        self.restart_node(0, self.extra_args[0] + ["-reindex-chainstate=1"])
        assert_equal(node.getbestblockhash(), tiphash)

        # 2. Restart the node  with bigger script ints disabled.
        #    We specify -reindex-chainstate=1 in order to have it re-evaluate all txns, and reject what it doesn't
        #    understand.  It should roll-back the latest block since now that block is invalid.
        self.restart_node(0, ["-acceptnonstdtxn=1", "-expire=0",
                              "-reindex-chainstate=1"])
        assert_equal(node.getbestblockhash(), prevtiphash)

        # 3. Finally, restart the node again with bigint64 enabled, and reconsider the block, it should now
        #    be accepted again
        self.restart_node(0, self.extra_args[0])
        assert_equal(node.getbestblockhash(), prevtiphash)
        node.reconsiderblock(tiphash)
        assert_equal(node.getbestblockhash(), tiphash)


if __name__ == '__main__':
    Int64CScriptNum().main()
