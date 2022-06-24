#!/usr/bin/env python3
# Copyright (c) 2014-2019 The Bitcoin Core developers
# Copyright (c) 2017-2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Run regression test suite.

This module calls down into individual test cases via subprocess. It will
forward all unrecognized arguments onto the individual test scripts.

Functional tests are disabled on Windows by default. Use --force to run them anyway.

For a description of arguments recognized by test scripts, see
`test/functional/test_framework/test_framework.py:BitcoinTestFramework.main`.

"""

import argparse
from collections import deque
import configparser
import datetime
import os
import time
import shutil
import sys
import subprocess
import tempfile
import re
import logging
import xml.etree.ElementTree as ET
import json
import threading
import multiprocessing
from queue import Queue, Empty

# Formatting. Default colors to empty strings.
BOLD, BLUE, RED, GREY = ("", ""), ("", ""), ("", ""), ("", "")
try:
    # Make sure python thinks it can write unicode to its stdout
    "\u2713".encode("utf_8").decode(sys.stdout.encoding)
    TICK = "✓ "
    CROSS = "✖ "
    CIRCLE = "○ "
except UnicodeDecodeError:
    TICK = "P "
    CROSS = "x "
    CIRCLE = "o "

if os.name == 'posix':
    # primitive formatting on supported
    # terminal via ANSI escape sequences:
    BOLD = ('\033[0m', '\033[1m')
    BLUE = ('\033[0m', '\033[0;34m')
    RED = ('\033[0m', '\033[0;31m')
    GREY = ('\033[0m', '\033[1;30m')

TEST_EXIT_PASSED = 0
TEST_EXIT_SKIPPED = 77

NON_SCRIPTS = [
    # These are python files that live in the functional tests directory, but
    # are not test scripts.
    "combine_logs.py",
    "create_cache.py",
    "test_runner.py",
]

TEST_PARAMS = {
    # Some test can be run with additional parameters.
    # When a test is listed here, the it  will be run without parameters
    # as well as with additional parameters listed here.
    # This:
    #    example "testName" : [["--param1", "--param2"] , ["--param3"]]
    # will run the test 3 times:
    #    testName
    #    testName --param1 --param2
    #    testname --param3
    "wallet_txn_doublespend.py": [["--mineblock"]],
    "wallet_txn_clone.py": [["--mineblock"]],
    "wallet_createwallet.py": [["--usecli"]],
    "wallet_multiwallet.py": [["--usecli"]],
    "radn-txbroadcastinterval.py": [["--testoutbound"]],
    "rpc_bind.py": [["--ipv4"], ["--ipv6"], ["--nonloopback"]],
}

# Used to limit the number of tests, when list of tests is not provided on command line
# When --extended is specified, we run all tests,
# otherwise we select tests based on execution time.
DEFAULT_CUTOFF = 40
DEFAULT_JOBS = (multiprocessing.cpu_count() // 3) + 1


class TestCase():
    """
    Data structure to hold and run information necessary to launch a test case.
    """

    def __init__(self, test_num, test_case, tests_dir,
                 tmpdir, failfast_event, flags=None):
        self.tests_dir = tests_dir
        self.tmpdir = tmpdir
        self.test_case = test_case
        self.test_num = test_num
        self.failfast_event = failfast_event
        self.flags = flags

    def run(self, portseed_offset):
        if self.failfast_event.is_set():
            return TestResult(self.test_num, self.test_case,
                              "", "Skipped", 0, "", "")

        portseed = self.test_num + portseed_offset
        portseed_arg = ["--portseed={}".format(portseed)]
        log_stdout = tempfile.SpooledTemporaryFile(max_size=2**16)
        log_stderr = tempfile.SpooledTemporaryFile(max_size=2**16)
        test_argv = self.test_case.split()
        testdir = os.path.join("{}", "{}_{}").format(
            self.tmpdir, re.sub(".py$", "", test_argv[0]), portseed)
        tmpdir_arg = ["--tmpdir={}".format(testdir)]
        time0 = time.time()
        process = subprocess.Popen([sys.executable, os.path.join(self.tests_dir, test_argv[0])] + test_argv[1:] + self.flags + portseed_arg + tmpdir_arg,
                                   universal_newlines=True,
                                   stdout=log_stdout,
                                   stderr=log_stderr)

        process.wait()
        log_stdout.seek(0), log_stderr.seek(0)
        [stdout, stderr] = [log.read().decode('utf-8')
                            for log in (log_stdout, log_stderr)]
        log_stdout.close(), log_stderr.close()
        if process.returncode == TEST_EXIT_PASSED and stderr == "":
            status = "Passed"
        elif process.returncode == TEST_EXIT_SKIPPED:
            status = "Skipped"
        else:
            status = "Failed"

        return TestResult(self.test_num, self.test_case, testdir, status,
                          int(time.time() - time0), stdout, stderr)


def on_ci():
    return os.getenv('TRAVIS') == 'true' or os.getenv(
        'TEAMCITY_VERSION') is not None


def main():
    # Read config generated by configure.
    config = configparser.ConfigParser()
    configfile = os.path.join(os.path.abspath(
        os.path.dirname(__file__)), "..", "config.ini")
    config.read_file(open(configfile, encoding="utf8"))

    src_dir = config["environment"]["SRCDIR"]
    build_dir = config["environment"]["BUILDDIR"]
    tests_dir = os.path.join(src_dir, 'test', 'functional')

    # Parse arguments and pass through unrecognised args
    parser = argparse.ArgumentParser(add_help=False,
                                     usage='%(prog)s [test_runner.py options] [script options] [scripts]',
                                     description=__doc__,
                                     epilog='''
    Help text and arguments for individual test script:''',
                                     formatter_class=argparse.ArgumentDefaultsHelpFormatter)
    parser.add_argument('--combinedlogslen', '-c', type=int, default=0,
                        help='Print a combined log (of length n lines) from all test nodes and test framework to the console on failure.')
    parser.add_argument('--coverage', action='store_true',
                        help='Generate a basic coverage report for the RPC interface.')
    parser.add_argument(
        '--exclude', '-x', help='Specify a comma-separated-list of tests to exclude.')
    parser.add_argument('--extended', action='store_true',
                        help='Run all tests in the test suite regardless of runtime. Ignores --cutoff and --startfrom.')
    parser.add_argument('--cutoff', type=int, default=DEFAULT_CUTOFF,
                        help='Skip tests with at least this runtime. Does not affect any new (i.e. untimed) tests.')
    parser.add_argument('--startfrom', type=int, default=argparse.SUPPRESS,
                        help='Only run tests with at least this runtime. Skips any new (i.e. untimed) tests. Ignores --cutoff.')
    parser.add_argument('--force', '-f', action='store_true',
                        help='Run tests even on platforms where they are disabled by default (e.g. Windows).')
    parser.add_argument('--help', '-h', '-?',
                        action='store_true', help='Show this help text and exit.')
    parser.add_argument('--jobs', '-j', type=int, default=DEFAULT_JOBS,
                        help='How many test scripts to run in parallel.')
    parser.add_argument('--keepcache', '-k', action='store_true',
                        help='The default behavior is to flush the cache directory on startup. --keepcache retains the cache from the previous testrun.')
    parser.add_argument('--quiet', '-q', action='store_true',
                        help='Only print results summary and failure logs.')
    parser.add_argument('--tmpdirprefix', '-t',
                        default=os.path.join(build_dir, 'test', 'tmp'), help="Root directory for datadirs")
    parser.add_argument(
        '--failfast',
        action='store_true',
        help='stop execution after the first test failure')
    parser.add_argument('--junitoutput', '-J', default='junit_results.xml',
                        help="File that will store JUnit formatted test results. If no absolute path is given it is treated as relative to the temporary directory.")
    parser.add_argument('--testsuitename', '-n', default='Radiant Node functional tests',
                        help="Name of the test suite, as it will appear in the logs and in the JUnit report.")
    args, unknown_args = parser.parse_known_args()

    # args to be passed on always start with two dashes; tests are the
    # remaining unknown args
    tests = [arg for arg in unknown_args if arg[:2] != "--"]
    passon_args = [arg for arg in unknown_args if arg[:2] == "--"]
    passon_args.append("--configfile={}".format(configfile))

    # Set up logging
    logging_level = logging.INFO if args.quiet else logging.DEBUG
    logging.basicConfig(format='%(message)s', level=logging_level)
    logging.info("Starting {}".format(args.testsuitename))

    # Create base test directory
    tmpdir = os.path.join("{}", "bitcoin_test_runner_{:%Y%m%d_%H%M%S}").format(
        args.tmpdirprefix, datetime.datetime.now())
    os.makedirs(tmpdir)

    logging.debug("Temporary test directory at {}".format(tmpdir))

    if not os.path.isabs(args.junitoutput):
        args.junitoutput = os.path.join(tmpdir, args.junitoutput)

    enable_bitcoind = config["components"].getboolean("ENABLE_BITCOIND")

    if config["environment"]["EXEEXT"] == ".exe" and not args.force:
        # https://github.com/bitcoin/bitcoin/commit/d52802551752140cf41f0d9a225a43e84404d3e9
        # https://github.com/bitcoin/bitcoin/pull/5677#issuecomment-136646964
        print(
            "Tests currently disabled on Windows by default. Use --force option to enable")
        sys.exit(0)

    if not enable_bitcoind:
        print("No functional tests to run.")
        print("Rerun ./configure with --with-daemon and then make")
        sys.exit(0)

    # Build list of tests
    all_scripts = get_all_scripts_from_disk(tests_dir, NON_SCRIPTS)

    # Check all tests with parameters actually exist
    for test in TEST_PARAMS:
        if test not in all_scripts:
            print("ERROR: Test with parameter {} does not exist, check it has "
                  "not been renamed or deleted".format(test))
            sys.exit(1)

    if tests:
        # Individual tests have been specified. Run specified tests that exist
        # in the all_scripts list. Accept the name with or without .py
        # extension.
        individual_tests = [
            re.sub(r"\.py$", "", t) + ".py" for t in tests if not t.endswith('*')]
        test_list = []
        for t in individual_tests:
            if t in all_scripts:
                test_list.append(t)
            else:
                print("{}WARNING!{} Test '{}' not found in full test list.".format(
                    BOLD[1], BOLD[0], t))

        # Allow for wildcard at the end of the name, so a single input can
        # match multiple tests
        for test in tests:
            if test.endswith('*'):
                test_list.extend(
                    [t for t in all_scripts if t.startswith(test[:-1])])

        # do not cut off explicitly specified tests
        cutoff = sys.maxsize
        startfrom = 0
    else:
        # No individual tests have been specified.
        # Run all tests that do not exceed
        test_list = all_scripts

        if args.extended:
            cutoff = sys.maxsize
            startfrom = 0
        elif 'startfrom' in args:
            cutoff = sys.maxsize
            startfrom = args.startfrom
        else:
            cutoff = args.cutoff
            startfrom = 0

    # Remove the test cases that the user has explicitly asked to exclude.
    if args.exclude:
        tests_excl = [re.sub(r"\.py$", "", t)
                      + ".py" for t in args.exclude.split(',')]
        for exclude_test in tests_excl:
            if exclude_test in test_list:
                test_list.remove(exclude_test)
            else:
                print("{}WARNING!{} Test '{}' not found in current test list.".format(
                    BOLD[1], BOLD[0], exclude_test))

    # Update timings from build_dir only if separate build directory is used.
    # We do not want to pollute source directory.
    build_timings = None
    if (src_dir != build_dir):
        build_timings = Timings(os.path.join(build_dir, 'timing.json'))

    # Always use timings from scr_dir if present
    src_timings = Timings(os.path.join(
        src_dir, "test", "functional", 'timing.json'))

    # Add test parameters and remove long running tests if needed
    test_list = get_tests_to_run(
        test_list, TEST_PARAMS, cutoff, startfrom, src_timings)

    if not test_list:
        print("No valid test scripts specified. Check that your test is in one "
              "of the test lists in test_runner.py, or run test_runner.py with no arguments to run all tests")
        sys.exit(0)

    if args.help:
        # Print help for test_runner.py, then print help of the first script
        # and exit.
        parser.print_help()
        subprocess.check_call(
            [sys.executable, os.path.join(tests_dir, test_list[0]), '-h'])
        sys.exit(0)

    check_script_prefixes(all_scripts)

    if not args.keepcache:
        shutil.rmtree(os.path.join(build_dir, "test",
                                   "cache"), ignore_errors=True)

    run_tests(
        test_list,
        build_dir,
        tests_dir,
        args.junitoutput,
        tmpdir,
        num_jobs=args.jobs,
        test_suite_name=args.testsuitename,
        enable_coverage=args.coverage,
        args=passon_args,
        combined_logs_len=args.combinedlogslen,
        build_timings=build_timings,
        failfast=args.failfast
    )


def run_tests(test_list, build_dir, tests_dir, junitoutput, tmpdir, num_jobs, test_suite_name,
              enable_coverage=False, args=None, combined_logs_len=0, build_timings=None, failfast=False):
    args = args or []

    # Warn if bitcoind is already running (unix only)
    try:
        pidofOutput = subprocess.check_output(["pidof", "bitcoind"])
        if pidofOutput is not None and pidofOutput != b'':
            print("{}WARNING!{} There is already a bitcoind process running on this system. Tests may fail unexpectedly due to resource contention!".format(
                BOLD[1], BOLD[0]))
    except (OSError, subprocess.SubprocessError):
        pass

    # Warn if there is a cache directory
    cache_dir = os.path.join(build_dir, "test", "cache")
    if os.path.isdir(cache_dir):
        print("{}WARNING!{} There is a cache directory here: {}. If tests fail unexpectedly, try deleting the cache directory.".format(
            BOLD[1], BOLD[0], cache_dir))

    flags = ['--cachedir={}'.format(cache_dir)] + args

    if enable_coverage:
        coverage = RPCCoverage()
        flags.append(coverage.flag)
        logging.debug(
            "Initializing coverage directory at {}".format(coverage.dir))
    else:
        coverage = None

    if len(test_list) > 1 and num_jobs > 1:
        # Populate cache
        try:
            subprocess.check_output([sys.executable, os.path.join(
                tests_dir, 'create_cache.py')] + flags + [os.path.join("--tmpdir={}", "cache") .format(tmpdir)])
        except subprocess.CalledProcessError as e:
            sys.stdout.buffer.write(e.output)
            raise

    # Run Tests
    time0 = time.time()
    test_results = execute_test_processes(
        num_jobs, test_list, tests_dir, tmpdir, flags, failfast)
    runtime = int(time.time() - time0)

    max_len_name = len(max(test_list, key=len))
    print_results(test_results, tests_dir, max_len_name,
                  runtime, combined_logs_len)
    save_results_as_junit(test_results, junitoutput, runtime, test_suite_name)

    if (build_timings is not None):
        build_timings.save_timings(test_results)

    if coverage:
        coverage_passed = coverage.report_rpc_coverage()

        logging.debug("Cleaning up coverage data")
        coverage.cleanup()
    else:
        coverage_passed = True

    # Clear up the temp directory if all subdirectories are gone
    if not os.listdir(tmpdir):
        os.rmdir(tmpdir)

    all_passed = all(map(
        lambda test_result: test_result.was_successful, test_results)) and coverage_passed

    sys.exit(not all_passed)


def execute_test_processes(
        num_jobs, test_list, tests_dir, tmpdir, flags, failfast=False):
    update_queue = Queue()
    job_queue = Queue()
    failfast_event = threading.Event()
    test_results = []
    poll_timeout = 10  # seconds
    # In case there is a graveyard of zombie bitcoinds, we can apply a
    # pseudorandom offset to hopefully jump over them.
    # (625 is PORT_RANGE/MAX_NODES)
    portseed_offset = int(time.time() * 1000) % 625

    ##
    # Define some helper functions we will need for threading.
    ##

    def handle_message(message, running_jobs):
        """
        handle_message handles a single message from handle_test_cases
        """
        if isinstance(message, TestCase):
            running_jobs.append((message.test_num, message.test_case))
            print("{}{}{} started".format(BOLD[1], message.test_case, BOLD[0]))
            return

        if isinstance(message, TestResult):
            test_result = message

            running_jobs.remove((test_result.num, test_result.name))
            test_results.append(test_result)

            if test_result.status == "Passed":
                print("{}{}{} passed, Duration: {} s".format(
                    BOLD[1], test_result.name, BOLD[0], test_result.time))
            elif test_result.status == "Skipped":
                print("{}{}{} skipped".format(
                    BOLD[1], test_result.name, BOLD[0]))
            else:
                print("{}{}{} failed, Duration: {} s\n".format(
                    BOLD[1], test_result.name, BOLD[0], test_result.time))
                print(BOLD[1] + 'stdout:' + BOLD[0])
                print(test_result.stdout)
                print(BOLD[1] + 'stderr:' + BOLD[0])
                print(test_result.stderr)

                if failfast:
                    logging.debug("Early exiting after test failure")
                    failfast_event.set()
            return

        assert False, "we should not be here"

    def handle_update_messages():
        """
        handle_update_messages waits for messages to be sent from handle_test_cases via the
        update_queue.  It serializes the results so we can print nice status update messages.
        """
        printed_status = False
        running_jobs = []

        while True:
            message = None
            try:
                message = update_queue.get(True, poll_timeout)
                if message is None:
                    break

                # We printed a status message, need to kick to the next line
                # before printing more.
                if printed_status:
                    print()
                    printed_status = False

                handle_message(message, running_jobs)
                update_queue.task_done()
            except Empty:
                if not on_ci():
                    print("Running jobs: {}".format(
                        ", ".join([j[1] for j in running_jobs])), end="\r")
                    sys.stdout.flush()
                    printed_status = True

    def handle_test_cases():
        """
        job_runner represents a single thread that is part of a worker pool.
        It waits for a test, then executes that test.
        It also reports start and result messages to handle_update_messages
        """
        while True:
            test = job_queue.get()
            if test is None:
                break
            # Signal that the test is starting to inform the poor waiting
            # programmer
            update_queue.put(test)
            result = test.run(portseed_offset)
            update_queue.put(result)
            job_queue.task_done()

    ##
    # Setup our threads, and start sending tasks
    ##

    # Start our result collection thread.
    resultCollector = threading.Thread(target=handle_update_messages)
    resultCollector.daemon = True
    resultCollector.start()

    # Start some worker threads
    for j in range(num_jobs):
        t = threading.Thread(target=handle_test_cases)
        t.daemon = True
        t.start()

    # Push all our test cases into the job queue.
    for i, t in enumerate(test_list):
        job_queue.put(TestCase(i, t, tests_dir, tmpdir, failfast_event, flags))

    # Wait for all the jobs to be completed
    job_queue.join()

    # Wait for all the results to be compiled
    update_queue.join()

    # Flush our queues so the threads exit
    update_queue.put(None)
    for j in range(num_jobs):
        job_queue.put(None)

    return test_results


def print_results(test_results, tests_dir, max_len_name,
                  runtime, combined_logs_len):
    results = "\n" + BOLD[1] + "{} | {} | {} | {}\n\n".format(
        "TEST".ljust(max_len_name), "STATUS   ", "DURATION", "ORIG. ORDER") + BOLD[0]

    test_results.sort(key=TestResult.sort_key)
    all_passed = True
    time_sum = 0

    for test_result in test_results:
        all_passed = all_passed and test_result.was_successful
        time_sum += test_result.time
        test_result.padding = max_len_name
        results += str(test_result)

        testdir = test_result.testdir
        if combined_logs_len and os.path.isdir(testdir):
            # Print the final `combinedlogslen` lines of the combined logs
            print('{}Combine the logs and print the last {} lines ...{}'.format(
                BOLD[1], combined_logs_len, BOLD[0]))
            print('\n============')
            print('{}Combined log for {}:{}'.format(BOLD[1], testdir, BOLD[0]))
            print('============\n')
            combined_logs, _ = subprocess.Popen([sys.executable, os.path.join(
                tests_dir, 'combine_logs.py'), '-c', testdir], universal_newlines=True, stdout=subprocess.PIPE).communicate()
            print(
                "\n".join(
                    deque(
                        combined_logs.splitlines(),
                        combined_logs_len)))

    status = TICK + "Passed" if all_passed else CROSS + "Failed"
    if not all_passed:
        results += RED[1]
    results += BOLD[1] + "\n{} | {} | {} s (accumulated)\n".format(
        "ALL".ljust(max_len_name), status.ljust(9), time_sum) + BOLD[0]
    if not all_passed:
        results += RED[0]
    results += "Runtime: {} s\n".format(runtime)
    print(results)


class TestResult():
    """
    Simple data structure to store test result values and print them properly
    """

    def __init__(self, num, name, testdir, status, time, stdout, stderr):
        self.num = num
        self.name = name
        self.testdir = testdir
        self.status = status
        self.time = time
        self.padding = 0
        self.stdout = stdout
        self.stderr = stderr

    def sort_key(self):
        if self.status == "Passed":
            return 0, self.name.lower()
        elif self.status == "Failed":
            return 2, self.name.lower()
        elif self.status == "Skipped":
            return 1, self.name.lower()

    def __repr__(self):
        if self.status == "Passed":
            color = BLUE
            glyph = TICK
        elif self.status == "Failed":
            color = RED
            glyph = CROSS
        elif self.status == "Skipped":
            color = GREY
            glyph = CIRCLE

        return color[1] + "{} | {}{} | {} | {}\n".format(
            self.name.ljust(self.padding), glyph, self.status.ljust(7), (str(self.time) + " s").ljust(8), self.num + 1) + color[0]

    @property
    def was_successful(self):
        return self.status != "Failed"


def get_all_scripts_from_disk(test_dir, non_scripts):
    """
    Return all available test script from script directory (excluding NON_SCRIPTS)
    """
    python_files = set([t for t in os.listdir(test_dir) if t[-3:] == ".py"])
    return list(python_files - set(non_scripts))


def check_script_prefixes(all_scripts):
    """Check that no more than `EXPECTED_VIOLATION_COUNT` of the
       test scripts don't start with one of the allowed name prefixes."""
    EXPECTED_VIOLATION_COUNT = 24

    # LEEWAY is provided as a transition measure, so that pull-requests
    # that introduce new tests that don't conform with the naming
    # convention don't immediately cause the tests to fail.
    LEEWAY = 10

    good_prefixes_re = re.compile(
        "(radn[_-])?(example|feature|interface|mempool|mining|p2p|rpc|wallet|tool)[_-]")
    bad_script_names = [
        script for script in all_scripts if good_prefixes_re.match(script) is None]

    if len(bad_script_names) < EXPECTED_VIOLATION_COUNT:
        print(
            "{}HURRAY!{} Number of functional tests violating naming convention reduced!".format(
                BOLD[1],
                BOLD[0]))
        print("Consider reducing EXPECTED_VIOLATION_COUNT from {} to {}".format(
            EXPECTED_VIOLATION_COUNT, len(bad_script_names)))
    elif len(bad_script_names) > EXPECTED_VIOLATION_COUNT:
        print(
            "INFO: {} tests not meeting naming conventions (expected {}):".format(len(bad_script_names), EXPECTED_VIOLATION_COUNT))
        print("  {}".format("\n  ".join(sorted(bad_script_names))))
        assert len(bad_script_names) <= EXPECTED_VIOLATION_COUNT + \
            LEEWAY, "Too many tests not following naming convention! ({} found, expected: <= {})".format(
                len(bad_script_names), EXPECTED_VIOLATION_COUNT)


def get_tests_to_run(test_list, test_params, cutoff, startfrom, src_timings):
    """
    Returns only test that will not run longer that cutoff.
    Returns only tests that will run at least startfrom.
    Long running tests are returned first to favor running tests in parallel
    Timings from build directory override those from src directory
    """

    def get_test_time(test):
        # Return 0 if test is unknown to always run it
        return next(
            (x['time'] for x in src_timings.existing_timings if x['name'] == test), 0)

    # Some tests must also be run with additional parameters. Add them to the
    # list.
    tests_with_params = []
    for test_name in test_list:
        # always execute a test without parameters
        tests_with_params.append(test_name)
        params = test_params.get(test_name)
        if params is not None:
            tests_with_params.extend(
                [test_name + " " + " ".join(p) for p in params])

    result = []
    for t in tests_with_params:
        runtime = get_test_time(t)
        if runtime < cutoff and runtime >= startfrom:
            result.append(t)

    result.sort(key=lambda x: (-get_test_time(x), x))
    return result


class RPCCoverage():
    """
    Coverage reporting utilities for test_runner.

    Coverage calculation works by having each test script subprocess write
    coverage files into a particular directory. These files contain the RPC
    commands invoked during testing, as well as a complete listing of RPC
    commands per `bitcoin-cli help` (`rpc_interface.txt`).

    After all tests complete, the commands run are combined and diff'd against
    the complete list to calculate uncovered RPC commands.

    See also: test/functional/test_framework/coverage.py

    """

    def __init__(self):
        self.dir = tempfile.mkdtemp(prefix="coverage")
        self.flag = '--coveragedir={}'.format(self.dir)

    def report_rpc_coverage(self):
        """
        Print out RPC commands that were unexercised by tests.

        """
        uncovered = self._get_uncovered_rpc_commands()

        if uncovered:
            print("Uncovered RPC commands:")
            print("".join(("  - {}\n".format(i)) for i in sorted(uncovered)))
            return False
        else:
            print("All RPC commands covered.")
            return True

    def cleanup(self):
        return shutil.rmtree(self.dir)

    def _get_uncovered_rpc_commands(self):
        """
        Return a set of currently untested RPC commands.

        """
        # This is shared from `test/functional/test-framework/coverage.py`
        reference_filename = 'rpc_interface.txt'
        coverage_file_prefix = 'coverage.'

        coverage_ref_filename = os.path.join(self.dir, reference_filename)
        coverage_filenames = set()
        all_cmds = set()
        covered_cmds = set()

        if not os.path.isfile(coverage_ref_filename):
            raise RuntimeError("No coverage reference found")

        with open(coverage_ref_filename, 'r', encoding="utf8") as f:
            all_cmds.update([i.strip() for i in f.readlines()])

        for root, dirs, files in os.walk(self.dir):
            for filename in files:
                if filename.startswith(coverage_file_prefix):
                    coverage_filenames.add(os.path.join(root, filename))

        for filename in coverage_filenames:
            with open(filename, 'r', encoding="utf8") as f:
                covered_cmds.update([i.strip() for i in f.readlines()])

        return all_cmds - covered_cmds


def save_results_as_junit(test_results, file_name, time, test_suite_name):
    """
    Save tests results to file in JUnit format

    See http://llg.cubic.org/docs/junit/ for specification of format
    """
    e_test_suite = ET.Element("testsuite",
                              {"name": "{}".format(test_suite_name),
                               "tests": str(len(test_results)),
                               # "errors":
                               "failures": str(len([t for t in test_results if t.status == "Failed"])),
                               "id": "0",
                               "skipped": str(len([t for t in test_results if t.status == "Skipped"])),
                               "time": str(time),
                               "timestamp": datetime.datetime.now().isoformat('T')
                               })

    for test_result in test_results:
        e_test_case = ET.SubElement(e_test_suite, "testcase",
                                    {"name": test_result.name,
                                     "classname": test_result.name,
                                     "time": str(test_result.time)
                                     }
                                    )
        if test_result.status == "Skipped":
            ET.SubElement(e_test_case, "skipped", {"message": "skipped"}).text = "skipped"
        elif test_result.status == "Failed":
            fail_result = test_result.stderr or test_result.stdout or "<no output>"
            ET.SubElement(e_test_case, "failure", {"message": "failure"}).text = fail_result
        # no special element for passed tests

        ET.SubElement(e_test_case, "system-out").text = test_result.stdout
        ET.SubElement(e_test_case, "system-err").text = test_result.stderr

    ET.ElementTree(e_test_suite).write(
        file_name, "UTF-8", xml_declaration=True)


class Timings():
    """
    Takes care of loading, merging and saving tests execution times.
    """

    def __init__(self, timing_file):
        self.timing_file = timing_file
        self.existing_timings = self.load_timings()

    def load_timings(self):
        if os.path.isfile(self.timing_file):
            with open(self.timing_file, encoding="utf8") as f:
                return json.load(f)
        else:
            return []

    def get_merged_timings(self, new_timings):
        """
        Return new list containing existing timings updated with new timings
        Tests that do not exists are not removed
        """

        key = 'name'
        merged = {}
        for item in self.existing_timings + new_timings:
            if item[key] in merged:
                merged[item[key]].update(item)
            else:
                merged[item[key]] = item

        # Sort the result to preserve test ordering in file
        merged = list(merged.values())
        merged.sort(key=lambda t, key=key: t[key])
        return merged

    def save_timings(self, test_results):
        # we only save test that have passed - timings for failed test might be
        # wrong (timeouts or early fails)
        passed_results = [t for t in test_results if t.status == 'Passed']
        new_timings = list(map(lambda t: {'name': t.name, 'time': t.time},
                               passed_results))
        merged_timings = self.get_merged_timings(new_timings)

        with open(self.timing_file, 'w', encoding="utf8") as f:
            json.dump(merged_timings, f, indent=True)


if __name__ == '__main__':
    main()
