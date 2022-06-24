#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Base class for RPC testing."""

import argparse
import configparser
from enum import Enum
import logging
import os
import pdb
import shutil
import sys
import tempfile
import time
from typing import Callable

from .authproxy import JSONRPCException
from . import coverage
from .test_node import TestNode
from .p2p import NetworkThread
from .util import (
    assert_equal,
    check_json_precision,
    connect_nodes_bi,
    disconnect_nodes,
    get_datadir_path,
    initialize_datadir,
    MAX_NODES,
    p2p_port,
    PortSeed,
    rpc_port,
    set_node_times,
)


class TestStatus(Enum):
    PASSED = 1
    FAILED = 2
    SKIPPED = 3


TEST_EXIT_PASSED = 0
TEST_EXIT_FAILED = 1
TEST_EXIT_SKIPPED = 77

# Timestamp is Dec. 1st, 2019 at 00:00:00
TIMESTAMP_IN_THE_PAST = 1575158400


class SkipTest(Exception):
    """This exception is raised to skip a test"""

    def __init__(self, message):
        self.message = message


class BitcoinTestMetaClass(type):
    """Metaclass for BitcoinTestFramework.

    Ensures that any attempt to register a subclass of `BitcoinTestFramework`
    adheres to a standard whereby the subclass overrides `set_test_params` and
    `run_test` but DOES NOT override either `__init__` or `main`. If any of
    those standards are violated, a ``TypeError`` is raised."""

    def __new__(cls, clsname, bases, dct):
        if not clsname == 'BitcoinTestFramework':
            if not ('run_test' in dct and 'set_test_params' in dct):
                raise TypeError("BitcoinTestFramework subclasses must override "
                                "'run_test' and 'set_test_params'")
            if '__init__' in dct or 'main' in dct:
                raise TypeError("BitcoinTestFramework subclasses may not override "
                                "'__init__' or 'main'")

        return super().__new__(cls, clsname, bases, dct)


class BitcoinTestFramework(metaclass=BitcoinTestMetaClass):
    """Base class for a bitcoin test script.

    Individual bitcoin test scripts should subclass this class and override the set_test_params() and run_test() methods.

    Individual tests can also override the following methods to customize the test setup:

    - add_options()
    - setup_chain()
    - setup_network()
    - setup_nodes()

    The __init__() and main() methods should not be overridden.

    This class also contains various public and private helper methods."""

    def __init__(self):
        """Sets test framework defaults. Do not override this method. Instead, override the set_test_params() method"""
        self.chain = 'regtest'
        self.setup_clean_chain = False
        self.nodes = []
        self.network_thread = None
        self.mocktime = 0
        # Wait for up to 60 seconds for the RPC server to respond
        self.rpc_timeout = 60
        self.supports_cli = False
        self.bind_to_localhost_only = True

    def main(self):
        """Main function. This should not be overridden by the subclass test scripts."""

        parser = argparse.ArgumentParser(usage="%(prog)s [options]")
        parser.add_argument("--nocleanup", dest="nocleanup", default=False, action="store_true",
                            help="Leave bitcoinds and test.* datadir on exit or error")
        parser.add_argument("--noshutdown", dest="noshutdown", default=False, action="store_true",
                            help="Don't stop bitcoinds after the test execution")
        parser.add_argument("--cachedir", dest="cachedir", default=os.path.abspath(os.path.dirname(os.path.realpath(__file__)) + "/../../cache"),
                            help="Directory for caching pregenerated datadirs (default: %(default)s)")
        parser.add_argument("--tmpdir", dest="tmpdir",
                            help="Root directory for datadirs")
        parser.add_argument("-l", "--loglevel", dest="loglevel", default="INFO",
                            help="log events at this level and higher to the console. Can be set to DEBUG, INFO, WARNING, ERROR or CRITICAL. Passing --loglevel DEBUG will output all logs to console. Note that logs at all levels are always written to the test_framework.log file in the temporary test directory.")
        parser.add_argument("--tracerpc", dest="trace_rpc", default=False, action="store_true",
                            help="Print out all RPC calls as they are made")
        parser.add_argument("--portseed", dest="port_seed", default=os.getpid(), type=int,
                            help="The seed to use for assigning port numbers (default: current process id)")
        parser.add_argument("--coveragedir", dest="coveragedir",
                            help="Write tested RPC commands into this directory")
        parser.add_argument("--configfile", dest="configfile", default=os.path.abspath(os.path.dirname(os.path.realpath(
            __file__)) + "/../../config.ini"), help="Location of the test framework config file (default: %(default)s)")
        parser.add_argument("--pdbonfailure", dest="pdbonfailure", default=False, action="store_true",
                            help="Attach a python debugger if test fails")
        parser.add_argument("--usecli", dest="usecli", default=False, action="store_true",
                            help="use bitcoin-cli instead of RPC for all commands")
        parser.add_argument("--with-upgrade8activation", dest="upgrade8activation", default=False, action="store_true",
                            help="Activate May 2022 (upgrade 8) update on timestamp {}".format(TIMESTAMP_IN_THE_PAST))
        parser.add_argument("--extra-bitcoind-args", dest="extra_bitcoind_args", default="",
                            help="Start bitcoind with these additional arguments (comma separated)")
        self.add_options(parser)
        self.options = parser.parse_args()

        self.set_test_params()
        assert hasattr(
            self, "num_nodes"), "Test must set self.num_nodes in set_test_params()"

        PortSeed.n = self.options.port_seed

        check_json_precision()

        self.options.cachedir = os.path.abspath(self.options.cachedir)

        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile, encoding='utf-8'))
        self.config = config
        self.options.bitcoind = os.getenv(
            "BITCOIND", default=config["environment"]["BUILDDIR"] + '/src/bitcoind' + config["environment"]["EXEEXT"])
        self.options.bitcoincli = os.getenv(
            "BITCOINCLI", default=config["environment"]["BUILDDIR"] + '/src/bitcoin-cli' + config["environment"]["EXEEXT"])
        self.options.emulator = config["environment"]["EMULATOR"] or None

        os.environ['PATH'] = config['environment']['BUILDDIR'] + os.pathsep + \
            config['environment']['BUILDDIR'] + os.path.sep + "qt" + os.pathsep + \
            os.environ['PATH']

        # Set up temp directory and start logging
        if self.options.tmpdir:
            self.options.tmpdir = os.path.abspath(self.options.tmpdir)
            os.makedirs(self.options.tmpdir, exist_ok=False)
        else:
            self.options.tmpdir = tempfile.mkdtemp(prefix="test")
        self._start_logging()

        self.log.debug('Setting up network thread')
        self.network_thread = NetworkThread()
        self.network_thread.start()

        success = TestStatus.FAILED

        try:
            if self.options.usecli:
                if not self.supports_cli:
                    raise SkipTest(
                        "--usecli specified but test does not support using CLI")
                self.skip_if_no_cli()
            self.skip_test_if_missing_module()
            self.setup_chain()
            self.setup_network()
            self.import_deterministic_coinbase_privkeys()
            self.run_test()
            success = TestStatus.PASSED
        except JSONRPCException:
            self.log.exception("JSONRPC error")
        except SkipTest as e:
            self.log.warning("Test Skipped: {}".format(e.message))
            success = TestStatus.SKIPPED
        except AssertionError:
            self.log.exception("Assertion failed")
        except KeyError:
            self.log.exception("Key error")
        except Exception:
            self.log.exception("Unexpected exception caught during testing")
        except KeyboardInterrupt:
            self.log.warning("Exiting after keyboard interrupt")

        if success == TestStatus.FAILED and self.options.pdbonfailure:
            print("Testcase failed. Attaching python debugger. Enter ? for help")
            pdb.set_trace()

        self.log.debug('Closing down network thread')
        self.network_thread.close()
        if not self.options.noshutdown:
            self.log.info("Stopping nodes")
            if self.nodes:
                self.stop_nodes()
        else:
            for node in self.nodes:
                node.cleanup_on_exit = False
            self.log.info(
                "Note: bitcoinds were not stopped and may still be running")

        if not self.options.nocleanup and not self.options.noshutdown and success != TestStatus.FAILED:
            self.log.info("Cleaning up {} on exit".format(self.options.tmpdir))
            cleanup_tree_on_exit = True
        else:
            self.log.warning(
                "Not cleaning up dir {}".format(self.options.tmpdir))
            cleanup_tree_on_exit = False

        if success == TestStatus.PASSED:
            self.log.info("Tests successful")
            exit_code = TEST_EXIT_PASSED
        elif success == TestStatus.SKIPPED:
            self.log.info("Test skipped")
            exit_code = TEST_EXIT_SKIPPED
        else:
            self.log.error(
                "Test failed. Test logging available at {}/test_framework.log".format(self.options.tmpdir))
            self.log.error("Hint: Call {} '{}' to consolidate all logs".format(os.path.normpath(
                os.path.dirname(os.path.realpath(__file__)) + "/../combine_logs.py"), self.options.tmpdir))
            exit_code = TEST_EXIT_FAILED
        logging.shutdown()
        if cleanup_tree_on_exit:
            shutil.rmtree(self.options.tmpdir)
        sys.exit(exit_code)

    # Methods to override in subclass test scripts.
    def set_test_params(self):
        """Tests must this method to change default values for number of nodes, topology, etc"""
        raise NotImplementedError

    def add_options(self, parser):
        """Override this method to add command-line options to the test"""
        pass

    def skip_test_if_missing_module(self):
        """Override this method to skip a test if a module is not compiled"""
        pass

    def setup_chain(self):
        """Override this method to customize blockchain setup"""
        self.log.info("Initializing test directory " + self.options.tmpdir)
        if self.setup_clean_chain:
            self._initialize_chain_clean()
        else:
            self._initialize_chain()

    def setup_network(self):
        """Override this method to customize test network topology"""
        self.setup_nodes()

        # Connect the nodes as a "chain".  This allows us
        # to split the network between nodes 1 and 2 to get
        # two halves that can work on competing chains.
        for i in range(self.num_nodes - 1):
            connect_nodes_bi(self.nodes[i], self.nodes[i + 1])
        self.sync_all()

    def setup_nodes(self):
        """Override this method to customize test node setup"""
        extra_args = None
        if hasattr(self, "extra_args"):
            extra_args = self.extra_args
        self.add_nodes(self.num_nodes, extra_args)
        self.start_nodes()

    def import_deterministic_coinbase_privkeys(self):
        if self.setup_clean_chain:
            return

        for n in self.nodes:
            try:
                n.getwalletinfo()
            except JSONRPCException as e:
                assert str(e).startswith('Method not found')
                continue

            n.importprivkey(n.get_deterministic_priv_key().key)

    def run_test(self):
        """Tests must override this method to define test logic"""
        raise NotImplementedError

    # Public helper methods. These can be accessed by the subclass test
    # scripts.

    def add_nodes(self, num_nodes, extra_args=None,
                  *, rpchost=None, binary=None):
        """Instantiate TestNode objects.

        Should only be called once after the nodes have been specified in
        set_test_params()."""
        if self.bind_to_localhost_only:
            extra_confs = [["bind=127.0.0.1"]] * num_nodes
        else:
            extra_confs = [[]] * num_nodes
        if extra_args is None:
            extra_args = [[]] * num_nodes
        if binary is None:
            binary = [self.options.bitcoind] * num_nodes
        assert_equal(len(extra_confs), num_nodes)
        assert_equal(len(extra_args), num_nodes)
        assert_equal(len(binary), num_nodes)
        for i in range(num_nodes):
            self.nodes.append(TestNode(
                i,
                get_datadir_path(self.options.tmpdir, i),
                chain=self.chain,
                host=rpchost,
                rpc_port=rpc_port(i),
                p2p_port=p2p_port(i),
                timewait=self.rpc_timeout,
                bitcoind=binary[i],
                bitcoin_cli=self.options.bitcoincli,
                mocktime=self.mocktime,
                coverage_dir=self.options.coveragedir,
                extra_conf=extra_confs[i],
                extra_args=extra_args[i],
                use_cli=self.options.usecli,
                emulator=self.options.emulator,
            ))
            if len(self.options.extra_bitcoind_args):
                self.nodes[i].extend_default_args(
                    self.options.extra_bitcoind_args.split(","))

    def start_node(self, i, *args, **kwargs):
        """Start a bitcoind"""

        node = self.nodes[i]

        node.start(*args, **kwargs)
        node.wait_for_rpc_connection()

        if self.options.coveragedir is not None:
            coverage.write_all_rpc_commands(self.options.coveragedir, node.rpc)

    def start_nodes(self, extra_args=None, *args, **kwargs):
        """Start multiple bitcoinds"""

        if extra_args is None:
            extra_args = [None] * self.num_nodes
        assert_equal(len(extra_args), self.num_nodes)
        try:
            for i, node in enumerate(self.nodes):
                node.start(extra_args[i], *args, **kwargs)
            for node in self.nodes:
                node.wait_for_rpc_connection()
        except BaseException:
            # If one node failed to start, stop the others
            self.stop_nodes()
            raise

        if self.options.coveragedir is not None:
            for node in self.nodes:
                coverage.write_all_rpc_commands(
                    self.options.coveragedir, node.rpc)

    def stop_node(self, i, expected_stderr='', wait=0):
        """Stop a bitcoind test node"""
        self.nodes[i].stop_node(expected_stderr, wait=wait)
        self.nodes[i].wait_until_stopped()

    def stop_nodes(self, wait=0):
        """Stop multiple bitcoind test nodes"""
        for node in self.nodes:
            # Issue RPC to stop nodes
            node.stop_node(wait=wait)

        for node in self.nodes:
            # Wait for nodes to stop
            node.wait_until_stopped()

    def restart_node(self, i, extra_args=None, before_start: Callable = None):
        """Stop and start a test node"""
        self.stop_node(i)
        if before_start:
            before_start()
        self.start_node(i, extra_args)

    def wait_for_node_exit(self, i, timeout):
        self.nodes[i].process.wait(timeout)

    def split_network(self):
        """
        Split the network of four nodes into nodes 0/1 and 2/3.
        """
        disconnect_nodes(self.nodes[1], self.nodes[2])
        disconnect_nodes(self.nodes[2], self.nodes[1])
        self.sync_all(self.nodes[:2])
        self.sync_all(self.nodes[2:])

    def join_network(self):
        """
        Join the (previously split) network halves together.
        """
        connect_nodes_bi(self.nodes[1], self.nodes[2])
        self.sync_all()

    def generate(self, generator, *args, **kwargs):
        blocks = generator.generate(*args, **kwargs)
        return blocks

    def generatetoaddress(self, generator, *args, **kwargs):
        blocks = generator.generatetoaddress(*args, **kwargs)
        return blocks

    def sync_blocks(self, nodes=None, wait=1, timeout=60):
        """
        Wait until everybody has the same tip.
        sync_blocks needs to be called with an rpc_connections set that has least
        one node already synced to the latest, stable tip, otherwise there's a
        chance it might return before all nodes are stably synced.
        """
        rpc_connections = nodes or self.nodes
        stop_time = time.time() + timeout
        while time.time() <= stop_time:
            best_hash = [x.getbestblockhash() for x in rpc_connections]
            if best_hash.count(best_hash[0]) == len(rpc_connections):
                return
            # Check that each peer has at least one connection
            assert (all([len(x.getpeerinfo()) for x in rpc_connections]))
            time.sleep(wait)
        raise AssertionError("Block sync timed out after {}s:{}".format(
            timeout,
            "".join("\n  {!r}".format(b) for b in best_hash),
        ))

    def sync_mempools(self, nodes=None, wait=1, timeout=60, flush_scheduler=True):
        """
        Wait until everybody has the same transactions in their memory
        pools
        """
        rpc_connections = nodes or self.nodes
        stop_time = time.time() + timeout
        while time.time() <= stop_time:
            pool = [set(r.getrawmempool()) for r in rpc_connections]
            if pool.count(pool[0]) == len(rpc_connections):
                if flush_scheduler:
                    for r in rpc_connections:
                        r.syncwithvalidationinterfacequeue()
                return
            # Check that each peer has at least one connection
            assert (all([len(x.getpeerinfo()) for x in rpc_connections]))
            time.sleep(wait)
        raise AssertionError("Mempool sync timed out after {}s:{}".format(
            timeout,
            "".join("\n  {!r}".format(m) for m in pool),
        ))

    def sync_all(self, nodes=None):
        self.sync_blocks(nodes)
        self.sync_mempools(nodes)

    # Private helper methods. These should not be accessed by the subclass
    # test scripts.

    def _start_logging(self):
        # Add logger and logging handlers
        self.log = logging.getLogger('TestFramework')
        self.log.setLevel(logging.DEBUG)
        # Create file handler to log all messages
        fh = logging.FileHandler(
            self.options.tmpdir + '/test_framework.log', encoding='utf-8')
        fh.setLevel(logging.DEBUG)
        # Create console handler to log messages to stderr. By default this
        # logs only error messages, but can be configured with --loglevel.
        ch = logging.StreamHandler(sys.stdout)
        # User can provide log level as a number or string (eg DEBUG). loglevel
        # was caught as a string, so try to convert it to an int
        ll = int(self.options.loglevel) if self.options.loglevel.isdigit(
        ) else self.options.loglevel.upper()
        ch.setLevel(ll)
        # Format logs the same as bitcoind's debug.log with microprecision (so
        # log files can be concatenated and sorted)
        formatter = logging.Formatter(
            fmt='%(asctime)s.%(msecs)03d000Z %(name)s (%(levelname)s): %(message)s', datefmt='%Y-%m-%dT%H:%M:%S')
        formatter.converter = time.gmtime
        fh.setFormatter(formatter)
        ch.setFormatter(formatter)
        # add the handlers to the logger
        self.log.addHandler(fh)
        self.log.addHandler(ch)

        if self.options.trace_rpc:
            rpc_logger = logging.getLogger("BitcoinRPC")
            rpc_logger.setLevel(logging.DEBUG)
            rpc_handler = logging.StreamHandler(sys.stdout)
            rpc_handler.setLevel(logging.DEBUG)
            rpc_logger.addHandler(rpc_handler)

    def _initialize_chain(self):
        """Initialize a pre-mined blockchain for use by the test.

        Create a cache of a 200-block-long chain (with wallet) for MAX_NODES
        Afterward, create num_nodes copies from the cache."""

        assert self.num_nodes <= MAX_NODES
        create_cache = False
        for i in range(MAX_NODES):
            if not os.path.isdir(get_datadir_path(self.options.cachedir, i)):
                create_cache = True
                break

        if create_cache:
            self.log.debug("Creating data directories from cached datadir")

            # find and delete old cache directories if any exist
            for i in range(MAX_NODES):
                if os.path.isdir(get_datadir_path(self.options.cachedir, i)):
                    shutil.rmtree(get_datadir_path(self.options.cachedir, i))

            # Create cache directories, run bitcoinds:
            for i in range(MAX_NODES):
                datadir = initialize_datadir(self.options.cachedir, i, self.chain)
                self.nodes.append(TestNode(
                    i,
                    get_datadir_path(self.options.cachedir, i),
                    chain=self.chain,
                    extra_conf=["bind=127.0.0.1"],
                    extra_args=[],
                    host=None,
                    rpc_port=rpc_port(i),
                    p2p_port=p2p_port(i),
                    timewait=self.rpc_timeout,
                    bitcoind=self.options.bitcoind,
                    bitcoin_cli=self.options.bitcoincli,
                    mocktime=self.mocktime,
                    coverage_dir=None,
                    emulator=self.options.emulator,
                ))
                self.nodes[i].clear_default_args()
                self.nodes[i].extend_default_args(["-datadir=" + datadir])
                self.nodes[i].extend_default_args(["-disablewallet"])
                if i > 0:
                    self.nodes[i].extend_default_args(
                        ["-connect=127.0.0.1:" + str(p2p_port(0))])
                self.start_node(i)

            # Wait for RPC connections to be ready
            for node in self.nodes:
                node.wait_for_rpc_connection()

            # For backward compatibility of the python scripts with previous
            # versions of the cache, set mocktime to Jan 1,
            # 2014 + (201 * 10 * 60)
            self.mocktime = 1388534400 + (201 * 10 * 60)

            # Create a 200-block-long chain; each of the 4 first nodes
            # gets 25 mature blocks and 25 immature.
            # Note: To preserve compatibility with older versions of
            # initialize_chain, only 4 nodes will generate coins.
            #
            # blocks are created with timestamps 10 minutes apart
            # starting from 2010 minutes in the past
            block_time = self.mocktime - (201 * 10 * 60)
            for i in range(2):
                for peer in range(4):
                    for j in range(25):
                        set_node_times(self.nodes, block_time)
                        self.generatetoaddress(self.nodes[peer],
                                               1, self.nodes[peer].get_deterministic_priv_key().address)
                        block_time += 10 * 60
                    # Must sync before next peer starts generating blocks
                    self.sync_blocks()

            # Shut them down, and clean up cache directories:
            self.stop_nodes()
            self.nodes = []
            self.mocktime = 0

            def cache_path(n, *paths):
                return os.path.join(get_datadir_path(self.options.cachedir, n), self.chain, *paths)

            for i in range(MAX_NODES):
                # Remove empty wallets dir
                os.rmdir(cache_path(i, 'wallets'))
                for entry in os.listdir(cache_path(i)):
                    entry_path = cache_path(i, entry)
                    if entry not in ['chainstate', 'blocks', 'gbt']:
                        os.remove(entry_path)
                    elif entry == 'gbt' and os.path.isdir(entry_path):
                        # Clear the gbt/ subtree since its semantics are "cache"
                        shutil.rmtree(entry_path)
                    del entry_path

        for i in range(self.num_nodes):
            from_dir = get_datadir_path(self.options.cachedir, i)
            to_dir = get_datadir_path(self.options.tmpdir, i)
            shutil.copytree(from_dir, to_dir)
            # Overwrite port/rpcport in bitcoin.conf
            initialize_datadir(self.options.tmpdir, i, self.chain)

    def _initialize_chain_clean(self):
        """Initialize empty blockchain for use by the test.

        Create an empty blockchain and num_nodes wallets.
        Useful if a test case wants complete control over initialization."""
        for i in range(self.num_nodes):
            initialize_datadir(self.options.tmpdir, i, self.chain)

    def skip_if_no_py3_zmq(self):
        """Attempt to import the zmq package and skip the test if the import fails."""
        try:
            import zmq  # noqa
        except ImportError:
            raise SkipTest("python3-zmq module not available.")

    def skip_if_no_bitcoind_zmq(self):
        """Skip the running test if bitcoind has not been compiled with zmq support."""
        if not self.is_zmq_compiled():
            raise SkipTest("bitcoind has not been built with zmq enabled.")

    def skip_if_no_wallet(self):
        """Skip the running test if wallet has not been compiled."""
        if not self.is_wallet_compiled():
            raise SkipTest("wallet has not been compiled.")

    def skip_if_no_cli(self):
        """Skip the running test if bitcoin-cli has not been compiled."""
        if not self.is_cli_compiled():
            raise SkipTest("bitcoin-cli has not been compiled.")

    def is_cli_compiled(self):
        """Checks whether bitcoin-cli was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile, encoding='utf-8'))

        return config["components"].getboolean("ENABLE_CLI")

    def is_wallet_compiled(self):
        """Checks whether the wallet module was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile, encoding='utf-8'))

        return config["components"].getboolean("ENABLE_WALLET")

    def is_zmq_compiled(self):
        """Checks whether the zmq module was compiled."""
        config = configparser.ConfigParser()
        config.read_file(open(self.options.configfile, encoding='utf-8'))

        return config["components"].getboolean("ENABLE_ZMQ")
