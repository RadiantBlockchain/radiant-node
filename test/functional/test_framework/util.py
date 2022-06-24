#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Helpful routines for regression testing."""

from base64 import b64encode
from binascii import unhexlify
from decimal import Decimal, ROUND_DOWN
from subprocess import CalledProcessError
import hashlib
import inspect
import json
import logging
import os
import random
import re
import time

from . import coverage
from .authproxy import AuthServiceProxy, JSONRPCException

logger = logging.getLogger("TestFramework.utils")

# Assert functions
##################


def assert_fee_amount(fee, tx_size, fee_per_kB, wiggleroom=2):
    """
    Assert the fee was in range

    wiggleroom defines an amount that the test expects the wallet to be off by
    when estimating fees.  This can be due to the dummy signature that is added
    during fee calculation, or due to the wallet funding transactions using the
    ceiling of the calculated fee.
    """
    target_fee = round(tx_size * fee_per_kB / 1000, 8)
    if fee < (tx_size - wiggleroom) * fee_per_kB / 1000:
        raise AssertionError(
            "Fee of {} RAD too low! (Should be {} BCH)".format(str(fee), str(target_fee)))
    if fee > (tx_size + wiggleroom) * fee_per_kB / 1000:
        raise AssertionError(
            "Fee of {} RAD too high! (Should be {} BCH)".format(str(fee), str(target_fee)))


def assert_equal(thing1, thing2, *args):
    if thing1 != thing2 or any(thing1 != arg for arg in args):
        raise AssertionError("not({})".format(" == ".join(str(arg)
                                                          for arg in (thing1, thing2) + args)))


def assert_not_equal(thing1, thing2, *args):
    if thing1 == thing2 or any(thing1 == arg for arg in args):
        raise AssertionError("not({})".format(" != ".join(str(arg)
                                                          for arg in (thing1, thing2) + args)))


def assert_greater_than(thing1, thing2):
    if thing1 <= thing2:
        raise AssertionError("{} <= {}".format(str(thing1), str(thing2)))


def assert_greater_than_or_equal(thing1, thing2):
    if thing1 < thing2:
        raise AssertionError("{} < {}".format(str(thing1), str(thing2)))


def assert_raises(exc, fun, *args, **kwds):
    assert_raises_message(exc, None, fun, *args, **kwds)


def assert_raises_message(exc, message, fun, *args, **kwds):
    try:
        fun(*args, **kwds)
    except JSONRPCException:
        raise AssertionError(
            "Use assert_raises_rpc_error() to test RPC failures")
    except exc as e:
        if message is not None and message not in e.error['message']:
            raise AssertionError(
                "Expected substring not found:" + e.error['message'])
    except Exception as e:
        raise AssertionError(
            "Unexpected exception raised: " + type(e).__name__)
    else:
        raise AssertionError("No exception raised")


def assert_raises_process_error(returncode, output, fun, *args, **kwds):
    """Execute a process and asserts the process return code and output.

    Calls function `fun` with arguments `args` and `kwds`. Catches a CalledProcessError
    and verifies that the return code and output are as expected. Throws AssertionError if
    no CalledProcessError was raised or if the return code and output are not as expected.

    Args:
        returncode (int): the process return code.
        output (string): [a substring of] the process output.
        fun (function): the function to call. This should execute a process.
        args*: positional arguments for the function.
        kwds**: named arguments for the function.
    """
    try:
        fun(*args, **kwds)
    except CalledProcessError as e:
        if returncode != e.returncode:
            raise AssertionError(
                "Unexpected returncode {}".format(e.returncode))
        if output not in e.output:
            raise AssertionError("Expected substring not found:" + e.output)
    else:
        raise AssertionError("No exception raised")


def assert_raises_rpc_error(code, message, fun, *args, **kwds):
    """Run an RPC and verify that a specific JSONRPC exception code and message is raised.

    Calls function `fun` with arguments `args` and `kwds`. Catches a JSONRPCException
    and verifies that the error code and message are as expected. Throws AssertionError if
    no JSONRPCException was raised or if the error code/message are not as expected.

    Args:
        code (int), optional: the error code returned by the RPC call (defined
            in src/rpc/protocol.h). Set to None if checking the error code is not required.
        message (string), optional: [a substring of] the error string returned by the
            RPC call. Set to None if checking the error string is not required.
        fun (function): the function to call. This should be the name of an RPC.
        args*: positional arguments for the function.
        kwds**: named arguments for the function.
    """
    assert try_rpc(code, message, fun, *args, **kwds), "No exception raised"


def try_rpc(code, message, fun, *args, **kwds):
    """Tries to run an rpc command.

    Test against error code and message if the rpc fails.
    Returns whether a JSONRPCException was raised."""
    try:
        fun(*args, **kwds)
    except JSONRPCException as e:
        # JSONRPCException was thrown as expected. Check the code and message
        # values are correct.
        if (code is not None) and (code != e.error["code"]):
            raise AssertionError(
                "Unexpected JSONRPC error code {}".format(e.error["code"]))
        if (message is not None) and (message not in e.error['message']):
            raise AssertionError(
                "Expected substring not found:" + e.error['message'])
        return True
    except Exception as e:
        raise AssertionError(
            "Unexpected exception raised: " + type(e).__name__)
    else:
        return False


def assert_is_hex_string(string):
    try:
        int(string, 16)
    except Exception as e:
        raise AssertionError(
            "Couldn't interpret {!r} as hexadecimal; raised: {}".format(string, e))


def assert_is_hash_string(string, length=64):
    if not isinstance(string, str):
        raise AssertionError(
            "Expected a string, got type {!r}".format(type(string)))
    elif length and len(string) != length:
        raise AssertionError(
            "String of length {} expected; got {}".format(length, len(string)))
    elif not re.match('[abcdef0-9]+$', string):
        raise AssertionError(
            "String {!r} contains invalid characters for a hash.".format(string))


def assert_array_result(object_array, to_match, expected,
                        should_not_find=False):
    """
    Pass in array of JSON objects, a dictionary with key/value pairs
    to match against, and another dictionary with expected key/value
    pairs.
    If the should_not_find flag is true, to_match should not be found
    in object_array
    """
    if should_not_find:
        assert_equal(expected, {})
    num_matched = 0
    for item in object_array:
        all_match = True
        for key, value in to_match.items():
            if key in item.keys():
                if item[key] != value:
                    all_match = False
            else:
                all_match = False
        if not all_match:
            continue
        elif should_not_find:
            num_matched = num_matched + 1
        for key, value in expected.items():
            if item[key] != value:
                raise AssertionError("{} : expected {}={}".format(
                    str(item), str(key), str(value)))
            num_matched = num_matched + 1
    if num_matched == 0 and not should_not_find:
        raise AssertionError("No objects matched {}".format(str(to_match)))
    if num_matched > 0 and should_not_find:
        raise AssertionError("Objects were found {}".format(str(to_match)))


def assert_blocktemplate_equal(blocktemplate0, blocktemplate1):
    """ Checks that the block templates are equal, except for the curtime.
    curtime is considered equal enough if the difference is within a second """
    curtime0 = int(blocktemplate0['curtime'])
    curtime1 = int(blocktemplate1['curtime'])
    curtimedelta = abs(curtime0 - curtime1)
    assert_greater_than_or_equal(1, curtimedelta)
    assert_equal(
        {i: blocktemplate0[i] for i in blocktemplate0 if i != 'curtime'},
        {i: blocktemplate1[i] for i in blocktemplate1 if i != 'curtime'}
    )

# Utility functions
###################


def check_json_precision():
    """Make sure json library being used does not lose precision converting RAD values"""
    n = Decimal("20000000.00000003")
    satoshis = int(json.loads(json.dumps(float(n))) * 1.0e8)
    if satoshis != 2000000000000003:
        raise RuntimeError("JSON encode/decode loses precision")


def count_bytes(hex_string):
    return len(bytearray.fromhex(hex_string))


def hash256_reversed(byte_str):
    """Compute sha256d, but return the result in big endian (reversed) byte order ready for hex display"""
    sha256 = hashlib.sha256()
    sha256.update(byte_str)
    sha256d = hashlib.sha256()
    sha256d.update(sha256.digest())
    return sha256d.digest()[::-1]


def hex_str_to_bytes(hex_str):
    return unhexlify(hex_str.encode('ascii'))


def str_to_b64str(string):
    return b64encode(string.encode('utf-8')).decode('ascii')


def satoshi_round(amount):
    return Decimal(amount).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)


def wait_until(predicate, *, attempts=float('inf'),
               timeout=float('inf'), lock=None):
    if attempts == float('inf') and timeout == float('inf'):
        timeout = 60
    attempt = 0
    time_end = time.time() + timeout

    while attempt < attempts and time.time() < time_end:
        if lock:
            with lock:
                if predicate():
                    return
        else:
            if predicate():
                return
        attempt += 1
        time.sleep(0.05)

    # Print the cause of the timeout
    predicate_source = "''''\n" + inspect.getsource(predicate) + "'''"
    logger.error("wait_until() failed. Predicate: {}".format(predicate_source))
    if attempt >= attempts:
        raise AssertionError("Predicate {} not true after {} attempts".format(
            predicate_source, attempts))
    elif time.time() >= time_end:
        raise AssertionError(
            "Predicate {} not true after {} seconds".format(predicate_source, timeout))
    raise RuntimeError('Unreachable')


CHAIN_CONF_ARG = {
    'testnet': 'testnet',
    'testnet3': 'testnet',
    'testnet4': 'testnet4',
    'scalenet': 'scalenet',
    'regtest': 'regtest',
}

CHAIN_CONF_SECTION = {
    'testnet': 'test',
    'testnet3': 'test',
    'testnet4': 'test4',
    'scalenet': 'scale',
    'regtest': 'regtest',
}


def get_chain_conf_arg(chain):
    return CHAIN_CONF_ARG.get(chain, None)


def get_chain_conf_section(chain):
    return CHAIN_CONF_SECTION.get(chain, None)

# RPC/P2P connection constants and functions
############################################


# The maximum number of nodes a single test can spawn
MAX_NODES = 8
# Don't assign rpc or p2p ports lower than this
PORT_MIN = int(os.getenv('TEST_RUNNER_PORT_MIN', default=11000))
# The number of ports to "reserve" for p2p and rpc, each
PORT_RANGE = 5000


class PortSeed:
    # Must be initialized with a unique integer for each process
    n = None


def get_rpc_proxy(url, node_number, timeout=None, coveragedir=None):
    """
    Args:
        url (str): URL of the RPC server to call
        node_number (int): the node number (or id) that this calls to

    Kwargs:
        timeout (int): HTTP timeout in seconds

    Returns:
        AuthServiceProxy. convenience object for making RPC calls.

    """
    proxy_kwargs = {}
    if timeout is not None:
        proxy_kwargs['timeout'] = timeout

    proxy = AuthServiceProxy(url, **proxy_kwargs)
    proxy.url = url  # store URL on proxy for info

    coverage_logfile = coverage.get_filename(
        coveragedir, node_number) if coveragedir else None

    return coverage.AuthServiceProxyWrapper(proxy, coverage_logfile)


def p2p_port(n):
    assert n <= MAX_NODES
    return PORT_MIN + n + \
        (MAX_NODES * PortSeed.n) % (PORT_RANGE - 1 - MAX_NODES)


def rpc_port(n):
    return PORT_MIN + PORT_RANGE + n + \
        (MAX_NODES * PortSeed.n) % (PORT_RANGE - 1 - MAX_NODES)


def rpc_url(datadir, chain, host, port):
    rpc_u, rpc_p = get_auth_cookie(datadir, chain)
    if host is None:
        host = '127.0.0.1'
    return "http://{}:{}@{}:{}".format(rpc_u, rpc_p, host, int(port))

# Node functions
################


def initialize_datadir(dirname, n, chain):
    datadir = get_datadir_path(dirname, n)
    if not os.path.isdir(datadir):
        os.makedirs(datadir)
    # Translate chain name to config name
    chain_name_conf_arg = get_chain_conf_arg(chain)
    chain_name_conf_section = get_chain_conf_section(chain)
    with open(os.path.join(datadir, "bitcoin.conf"), 'w', encoding='utf8') as f:
        if chain_name_conf_arg:
            f.write("{}=1\n".format(chain_name_conf_arg))
            f.write("[{}]\n".format(chain_name_conf_section))
        f.write("port=" + str(p2p_port(n)) + "\n")
        f.write("rpcport=" + str(rpc_port(n)) + "\n")
        f.write("server=1\n")
        f.write("keypool=1\n")
        f.write("discover=0\n")
        f.write("dnsseed=0\n")
        f.write("listenonion=0\n")
        f.write("usecashaddr=1\n")
        os.makedirs(os.path.join(datadir, 'stderr'), exist_ok=True)
        os.makedirs(os.path.join(datadir, 'stdout'), exist_ok=True)
    return datadir


def get_datadir_path(dirname, n):
    return os.path.join(dirname, "node" + str(n))


def append_config(datadir, options):
    with open(os.path.join(datadir, "bitcoin.conf"), 'a', encoding='utf8') as f:
        for option in options:
            f.write(option + "\n")


def get_auth_cookie(datadir, chain):
    user = None
    password = None
    if os.path.isfile(os.path.join(datadir, "bitcoin.conf")):
        with open(os.path.join(datadir, "bitcoin.conf"), 'r', encoding='utf8') as f:
            for line in f:
                if line.startswith("rpcuser="):
                    assert user is None  # Ensure that there is only one rpcuser line
                    user = line.split("=")[1].strip("\n")
                if line.startswith("rpcpassword="):
                    assert password is None  # Ensure that there is only one rpcpassword line
                    password = line.split("=")[1].strip("\n")
    try:
        with open(os.path.join(datadir, chain, ".cookie"), 'r', encoding="ascii") as f:
            userpass = f.read()
            split_userpass = userpass.split(':')
            user = split_userpass[0]
            password = split_userpass[1]
    except OSError:
        pass
    if user is None or password is None:
        raise ValueError("No RPC credentials")
    return user, password


# If a cookie file exists in the given datadir, delete it.
def delete_cookie_file(datadir, chain):
    if os.path.isfile(os.path.join(datadir, chain, ".cookie")):
        logger.debug("Deleting leftover cookie file")
        os.remove(os.path.join(datadir, chain, ".cookie"))


def set_node_times(nodes, t):
    for node in nodes:
        node.setmocktime(t)


def disconnect_nodes(from_node, to_node):
    for peer_id in [peer['id'] for peer in from_node.getpeerinfo(
    ) if to_node.name in peer['subver']]:
        try:
            from_node.disconnectnode(nodeid=peer_id)
        except JSONRPCException as e:
            # If this node is disconnected between calculating the peer id
            # and issuing the disconnect, don't worry about it.
            # This avoids a race condition if we're mass-disconnecting peers.
            if e.error['code'] != -29:  # RPC_CLIENT_NODE_NOT_CONNECTED
                raise

    # wait to disconnect
    wait_until(lambda: [peer['id'] for peer in from_node.getpeerinfo(
    ) if to_node.name in peer['subver']] == [], timeout=5)


def connect_nodes(from_node, to_node):
    host = to_node.host
    if host is None:
        host = '127.0.0.1'
    ip_port = host + ':' + str(to_node.p2p_port)
    from_node.addnode(ip_port, "onetry")
    # poll until version handshake complete to avoid race conditions
    # with transaction relaying
    wait_until(lambda: all(peer['version']
                           != 0 for peer in from_node.getpeerinfo()))


def connect_nodes_bi(a, b):
    connect_nodes(a, b)
    connect_nodes(b, a)


# Transaction/Block functions
#############################


def find_output(node, txid, amount):
    """
    Return index to output of txid with value amount
    Raises exception if there is none.
    """
    txdata = node.getrawtransaction(txid, 1)
    for i in range(len(txdata["vout"])):
        if txdata["vout"][i]["value"] == amount:
            return i
    raise RuntimeError("find_output txid {} : {} not found".format(
        txid, str(amount)))


def gather_inputs(from_node, amount_needed, confirmations_required=1):
    """
    Return a random set of unspent txouts that are enough to pay amount_needed
    """
    assert confirmations_required >= 0
    utxo = from_node.listunspent(confirmations_required)
    random.shuffle(utxo)
    inputs = []
    total_in = Decimal("0.00000000")
    while total_in < amount_needed and len(utxo) > 0:
        t = utxo.pop()
        total_in += t["amount"]
        inputs.append(
            {"txid": t["txid"], "vout": t["vout"], "address": t["address"]})
    if total_in < amount_needed:
        raise RuntimeError("Insufficient funds: need {}, have {}".format(
            amount_needed, total_in))
    return (total_in, inputs)


def make_change(from_node, amount_in, amount_out, fee):
    """
    Create change output(s), return them
    """
    outputs = {}
    amount = amount_out + fee
    change = amount_in - amount
    if change > amount * 2:
        # Create an extra change output to break up big inputs
        change_address = from_node.getnewaddress()
        # Split change in two, being careful of rounding:
        outputs[change_address] = Decimal(
            change / 2).quantize(Decimal('0.00000001'), rounding=ROUND_DOWN)
        change = amount_in - amount - outputs[change_address]
    if change > 0:
        outputs[from_node.getnewaddress()] = change
    return outputs


def random_transaction(nodes, amount, min_fee, fee_increment, fee_variants):
    """
    Create a random transaction.
    Returns (txid, hex-encoded-transaction-data, fee)
    """
    from_node = random.choice(nodes)
    to_node = random.choice(nodes)
    fee = min_fee + fee_increment * random.randint(0, fee_variants)

    (total_in, inputs) = gather_inputs(from_node, amount + fee)
    outputs = make_change(from_node, total_in, amount, fee)
    outputs[to_node.getnewaddress()] = float(amount)

    rawtx = from_node.createrawtransaction(inputs, outputs)
    signresult = from_node.signrawtransactionwithwallet(rawtx)
    txid = from_node.sendrawtransaction(signresult["hex"], True)

    return (txid, signresult["hex"], fee)

# Create large OP_RETURN txouts that can be appended to a transaction
# to make it large (helper for constructing large transactions).


def gen_return_txouts():
    # Some pre-processing to create a bunch of OP_RETURN txouts to insert into transactions we create
    # So we have big transactions (and therefore can't fit very many into each block)
    # create one script_pubkey
    script_pubkey = "6a4d0200"  # OP_RETURN OP_PUSH2 512 bytes
    for i in range(512):
        script_pubkey = script_pubkey + "01"
    # concatenate 128 txouts of above script_pubkey which we'll insert before
    # the txout for change
    txouts = "81"
    for k in range(128):
        # add txout value
        txouts = txouts + "0000000000000000"
        # add length of script_pubkey
        txouts = txouts + "fd0402"
        # add script_pubkey
        txouts = txouts + script_pubkey
    return txouts

# Create a spend of each passed-in utxo, splicing in "txouts" to each raw
# transaction to make it large.  See gen_return_txouts() above.


def create_lots_of_big_transactions(node, txouts, utxos, num, fee):
    addr = node.getnewaddress()
    txids = []
    for _ in range(num):
        t = utxos.pop()
        inputs = [{"txid": t["txid"], "vout": t["vout"]}]
        outputs = {}
        change = t['amount'] - fee
        outputs[addr] = satoshi_round(change)
        rawtx = node.createrawtransaction(inputs, outputs)
        newtx = rawtx[0:92]
        newtx = newtx + txouts
        newtx = newtx + rawtx[94:]
        signresult = node.signrawtransactionwithwallet(
            newtx, None, "NONE|FORKID")
        txid = node.sendrawtransaction(signresult["hex"], True)
        txids.append(txid)
    return txids


def find_vout_for_address(node, txid, addr):
    """
    Locate the vout index of the given transaction sending to the
    given address. Raises runtime error exception if not found.
    """
    tx = node.getrawtransaction(txid, True)
    for i in range(len(tx["vout"])):
        if any([addr == a for a in tx["vout"][i]["scriptPubKey"]["addresses"]]):
            return i
    raise RuntimeError(
        "Vout not found for address: txid={}, addr={}".format(txid, addr))
