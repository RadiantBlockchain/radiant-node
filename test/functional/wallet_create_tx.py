#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_raises_rpc_error, assert_not_equal
)


class CreateTxWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = False
        self.num_nodes = 2

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    @staticmethod
    def validate_inputs_bip69(inputs):
        last_hash = ""
        last_n = 0
        first = False
        for tx_input in inputs:
            if (first == False):
                first = True
                last_hash = tx_input["txid"]
                last_n = tx_input["vout"]
                continue
            if last_hash > tx_input["txid"]:
                return False
            if last_hash == tx_input["txid"]:
                if last_n > tx_input["vout"]:
                    return False
            last_hash = tx_input["txid"]
            last_n = tx_input["vout"]
        return True

    @staticmethod
    def validate_outputs_bip69(outputs):
        last_value = 0
        last_pubkey = ""
        first = False
        for tx_output in outputs:
            if (first == False):
                first = True
                last_value = tx_output["value"]
                last_pubkey = tx_output["scriptPubKey"]["hex"]
                continue
            if last_value > tx_output["value"]:
                return False
            if last_value == tx_output["value"]:
                if last_pubkey > tx_output["scriptPubKey"]["hex"]:
                    return False
            last_value = tx_output["value"]
            last_pubkey = tx_output["scriptPubKey"]["hex"]
        return True

    def test_send_unsafe_inputs(self):
        """ Make sure unsafe inputs are included if specified """
        self.log.info("Testing sendtoaddress and sendmany - include_unsafe (6th arg)")
        self.nodes[0].createwallet(wallet_name="unsafe")
        wunsafe = self.nodes[0].get_wallet_rpc("unsafe")
        assert_equal(wunsafe.getbalance(), 0)
        assert self.nodes[1].getbalance() >= 2
        outaddr = self.nodes[1].getnewaddress()

        # Fund node0 "unsafe" wallet with unconfirmed coin(s)
        self.nodes[1].sendtoaddress(wunsafe.getnewaddress(), 1)
        self.sync_mempools(self.nodes[0:2])
        assert_raises_rpc_error(-6, "Insufficient funds", wunsafe.sendtoaddress, outaddr, 1,
                                '', '', True, 0)  # default include_unsafe = False
        # Sending with the include_unsafe option will spend the unconfirmed external coin
        wunsafe.sendtoaddress(outaddr, 1,
                              '', '', True, 0, True)  # include_unsafe = True
        self.generate(self.nodes[1], 1)  # commit tx
        self.sync_all()
        assert_equal(wunsafe.getbalance(), 0)

        # Try the above with sendmany which also should support include_unsafe
        self.nodes[1].sendtoaddress(wunsafe.getnewaddress(), 1)
        self.sync_mempools(self.nodes[0:2])
        assert_equal(wunsafe.getbalance(), 0)  # 0 available, but this only lists "safe"
        assert_raises_rpc_error(-6, "Insufficient funds", wunsafe.sendmany, '', {outaddr: 1},
                                0, '', [outaddr], 0)  # default include_unsafe = False
        # Sending with the include_unsafe option will spend the unconfirmed external coin
        wunsafe.sendmany('', {outaddr: 1},
                         0, '', [outaddr], 0, True)  # include_unsafe = True

    def run_test(self):

        # Sending exactly 50 coins and subtracting fee from amount should always
        # result in a tx with exactly 1 input and 1 output. A list of 1 is always
        # sorted but check anyway. We run this check first to ensure we do not
        # randomly select and output that is less than 50 because it has been used
        # for something else in this test
        self.log.info('Check that a tx with 1 input and 1 output is BIP69 sorted')
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 50, "", "", True)
        tx = self.nodes[0].decoderawtransaction(
            self.nodes[0].gettransaction(txid)['hex'])
        tx_vin = tx["vin"]
        assert_equal(len(tx_vin), 1)
        tx_vout = tx["vout"]
        assert_equal(len(tx_vout), 1)
        assert_equal(self.validate_inputs_bip69(tx_vin), True)
        assert_equal(self.validate_outputs_bip69(tx_vout), True)

        self.log.info('Check that a tx with >1 input and >1 output is BIP69 sorted')
        outputs = {self.nodes[0].getnewaddress(): 110, self.nodes[0].getnewaddress(): 1.2, self.nodes[0].getnewaddress(): 35, self.nodes[
            0].getnewaddress(): 1.3, self.nodes[0].getnewaddress(): 20, self.nodes[0].getnewaddress(): 0.3}
        txid = self.nodes[0].sendmany("", outputs)
        tx = self.nodes[0].decoderawtransaction(
            self.nodes[0].gettransaction(txid)['hex'])
        tx_vin = tx["vin"]
        # It is not necessary to check for len of 0 because it is not possible.
        # The number of inputs is variable, as long as it is not 1 or 0 this test
        # is behaving as intended
        assert_not_equal(len(tx_vin), 1)
        tx_vout = tx["vout"]
        # There should be 7 outputs, 6 specified sends and 1 change
        assert_equal(len(tx_vout), 7)
        assert_equal(self.validate_inputs_bip69(tx_vin), True)
        assert_equal(self.validate_outputs_bip69(tx_vout), True)

        self.log.info(
            'Check that we have some (old) blocks and that anti-fee-sniping is disabled')
        assert_equal(self.nodes[0].getblockchaininfo()['blocks'], 200)
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        tx = self.nodes[0].decoderawtransaction(
            self.nodes[0].gettransaction(txid)['hex'])
        assert_equal(tx['locktime'], 0)

        self.log.info(
            'Check that anti-fee-sniping is enabled when we mine a recent block')
        self.generate(self.nodes[0], 1)
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        tx = self.nodes[0].decoderawtransaction(
            self.nodes[0].gettransaction(txid)['hex'])
        assert 0 < tx['locktime'] <= 201

        # Ensure the 'include_unsafe' option works for sendmany and sendtoaddress RPCs
        self.test_send_unsafe_inputs()


if __name__ == '__main__':
    CreateTxWalletTest().main()
