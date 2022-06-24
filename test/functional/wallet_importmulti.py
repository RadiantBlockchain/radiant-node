#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the importmulti RPC."""

from test_framework.script import (
    CScript,
    OP_NOP,
)
from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import (
    assert_equal,
    assert_greater_than,
    assert_raises_rpc_error,
)


class ImportMultiTest (BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 2
        self.setup_clean_chain = True

    def skip_test_if_missing_module(self):
        self.skip_if_no_wallet()

    def setup_network(self, split=False):
        self.setup_nodes()

    def run_test(self):
        self.log.info("Mining blocks...")
        self.generate(self.nodes[0], 1)
        self.generate(self.nodes[1], 1)
        timestamp = self.nodes[1].getblock(
            self.nodes[1].getbestblockhash())['mediantime']

        node0_address1 = self.nodes[0].getaddressinfo(
            self.nodes[0].getnewaddress())

        # Check only one address
        assert_equal(node0_address1['ismine'], True)

        # Node 1 sync test
        assert_equal(self.nodes[1].getblockcount(), 1)

        # Address Test - before import
        address_info = self.nodes[1].getaddressinfo(node0_address1['address'])
        assert_equal(address_info['iswatchonly'], False)
        assert_equal(address_info['ismine'], False)

        # RPC importmulti -----------------------------------------------

        # Bitcoin Address (implicit non-internal)
        self.log.info("Should import an address")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
        }])
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], True)
        assert_equal(address_assert['ismine'], False)
        assert_equal(address_assert['timestamp'], timestamp)
        assert_equal(address_assert['ischange'], False)
        watchonly_address = address['address']
        watchonly_timestamp = timestamp

        self.log.info("Should not import an invalid address")
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": "not valid address",
            },
            "timestamp": "now",
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -5)
        assert_equal(result[0]['error']['message'], 'Invalid address')

        # ScriptPubKey + internal
        self.log.info("Should import a scriptPubKey with internal flag")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": address['scriptPubKey'],
            "timestamp": "now",
            "internal": True
        }])
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], True)
        assert_equal(address_assert['ismine'], False)
        assert_equal(address_assert['timestamp'], timestamp)
        assert_equal(address_assert['ischange'], True)

        # Nonstandard scriptPubKey + !internal
        self.log.info(
            "Should not import a nonstandard scriptPubKey without internal flag")
        nonstandardScriptPubKey = address['scriptPubKey'] + \
            CScript([OP_NOP]).hex()
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": nonstandardScriptPubKey,
            "timestamp": "now",
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -8)
        assert_equal(
            result[0]['error']['message'],
            'Internal must be set to true for nonstandard scriptPubKey imports.')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # Address + Public key + !Internal(explicit)
        self.log.info("Should import an address with public key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
            "pubkeys": [address['pubkey']],
            "internal": False
        }])
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], True)
        assert_equal(address_assert['ismine'], False)
        assert_equal(address_assert['timestamp'], timestamp)

        # ScriptPubKey + Public key + internal
        self.log.info(
            "Should import a scriptPubKey with internal and with public key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        request = [{
            "scriptPubKey": address['scriptPubKey'],
            "timestamp": "now",
            "pubkeys": [address['pubkey']],
            "internal": True
        }]
        result = self.nodes[1].importmulti(requests=request)
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], True)
        assert_equal(address_assert['ismine'], False)
        assert_equal(address_assert['timestamp'], timestamp)

        # Nonstandard scriptPubKey + Public key + !internal
        self.log.info(
            "Should not import a nonstandard scriptPubKey without internal and with public key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        request = [{
            "scriptPubKey": nonstandardScriptPubKey,
            "timestamp": "now",
            "pubkeys": [address['pubkey']]
        }]
        result = self.nodes[1].importmulti(requests=request)
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -8)
        assert_equal(
            result[0]['error']['message'],
            'Internal must be set to true for nonstandard scriptPubKey imports.')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # Address + Private key + !watchonly
        self.log.info("Should import an address with private key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address['address'])]
        }])
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], True)
        assert_equal(address_assert['timestamp'], timestamp)

        self.log.info(
            "Should not import an address with private key if is already imported")
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address['address'])]
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -4)
        assert_equal(result[0]['error']['message'],
                     'The wallet already contains the private key for this address or script')

        # Address + Private key + watchonly
        self.log.info(
            "Should not import an address with private key and with watchonly")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address['address'])],
            "watchonly": True
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -8)
        assert_equal(result[0]['error']['message'],
                     'Incompatibility found between watchonly and keys')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # ScriptPubKey + Private key + internal
        self.log.info(
            "Should import a scriptPubKey with internal and with private key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": address['scriptPubKey'],
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address['address'])],
            "internal": True
        }])
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], True)
        assert_equal(address_assert['timestamp'], timestamp)

        # Nonstandard scriptPubKey + Private key + !internal
        self.log.info(
            "Should not import a nonstandard scriptPubKey without internal and with private key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": nonstandardScriptPubKey,
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address['address'])]
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -8)
        assert_equal(
            result[0]['error']['message'],
            'Internal must be set to true for nonstandard scriptPubKey imports.')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)
 
        # Address + Public key + !Internal + Wrong pubkey
        self.log.info("Should not import an address with a wrong public key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        address2 = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
            "pubkeys": [address2['pubkey']]
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -5)
        assert_equal(result[0]['error']['message'], 'Consistency check failed')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # ScriptPubKey + Public key + internal + Wrong pubkey
        self.log.info(
            "Should not import a scriptPubKey with internal and with a wrong public key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        address2 = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        request = [{
            "scriptPubKey": address['scriptPubKey'],
            "timestamp": "now",
            "pubkeys": [address2['pubkey']],
            "internal": True
        }]
        result = self.nodes[1].importmulti(request)
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -5)
        assert_equal(result[0]['error']['message'], 'Consistency check failed')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # Address + Private key + !watchonly + Wrong private key
        self.log.info("Should not import an address with a wrong private key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        address2 = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": address['address']
            },
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address2['address'])]
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -5)
        assert_equal(result[0]['error']['message'], 'Consistency check failed')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # ScriptPubKey + Private key + internal + Wrong private key
        self.log.info(
            "Should not import a scriptPubKey with internal and with a wrong private key")
        address = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        address2 = self.nodes[0].getaddressinfo(self.nodes[0].getnewaddress())
        result = self.nodes[1].importmulti([{
            "scriptPubKey": address['scriptPubKey'],
            "timestamp": "now",
            "keys": [self.nodes[0].dumpprivkey(address2['address'])],
            "internal": True
        }])
        assert_equal(result[0]['success'], False)
        assert_equal(result[0]['error']['code'], -5)
        assert_equal(result[0]['error']['message'], 'Consistency check failed')
        address_assert = self.nodes[1].getaddressinfo(address['address'])
        assert_equal(address_assert['iswatchonly'], False)
        assert_equal(address_assert['ismine'], False)
        assert_equal('timestamp' in address_assert, False)

        # Importing existing watch only address with new timestamp should
        # replace saved timestamp.
        assert_greater_than(timestamp, watchonly_timestamp)
        self.log.info("Should replace previously saved watch only timestamp.")
        result = self.nodes[1].importmulti([{
            "scriptPubKey": {
                "address": watchonly_address,
            },
            "timestamp": "now",
        }])
        assert_equal(result[0]['success'], True)
        address_assert = self.nodes[1].getaddressinfo(watchonly_address)
        assert_equal(address_assert['iswatchonly'], True)
        assert_equal(address_assert['ismine'], False)
        assert_equal(address_assert['timestamp'], timestamp)
        watchonly_timestamp = timestamp

        # restart nodes to check for proper serialization/deserialization of
        # watch only address
        self.stop_nodes()
        self.start_nodes()
        address_assert = self.nodes[1].getaddressinfo(watchonly_address)
        assert_equal(address_assert['iswatchonly'], True)
        assert_equal(address_assert['ismine'], False)
        assert_equal(address_assert['timestamp'], watchonly_timestamp)

        # Bad or missing timestamps
        self.log.info("Should throw on invalid or missing timestamp values")
        assert_raises_rpc_error(-3, 'Missing required timestamp field for key',
                                self.nodes[1].importmulti, [{
                                    "scriptPubKey": address['scriptPubKey'],
                                }])
        assert_raises_rpc_error(-3, 'Expected number or "now" timestamp value for key. got type string',
                                self.nodes[1].importmulti, [{
                                    "scriptPubKey": address['scriptPubKey'],
                                    "timestamp": "",
                                }])


if __name__ == '__main__':
    ImportMultiTest().main()
