#!/usr/bin/env python3
# Copyright (c) 2018 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal, assert_not_equal
)


class CreateTxWalletTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = False
        self.num_nodes = 1

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def validate_inputs_bip69(self, inputs):
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

    def validate_outputs_bip69(self, outputs):
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
        self.nodes[0].generate(1)
        txid = self.nodes[0].sendtoaddress(self.nodes[0].getnewaddress(), 1)
        tx = self.nodes[0].decoderawtransaction(
            self.nodes[0].gettransaction(txid)['hex'])
        assert 0 < tx['locktime'] <= 201


if __name__ == '__main__':
    CreateTxWalletTest().main()
