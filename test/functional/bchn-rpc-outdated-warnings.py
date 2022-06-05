#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
import time
import os

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import wait_until

DUMMY_ACTIVATION_TIME = 2000000000

# Warning should show up this many seconds before activation.
OUTDATED_WARN_START = 3600 * 24 * 30  # 30 days.
WARNING_TEXT_SOON = "Warning: This version of Radiant Node is old and may fall out of network consensus in"
WARNING_TEXT_EXPIRED = "Warning: This version of Radiant Node is old and may have fallen out of network consensus 2 day(s) ago"
# The amount of "lead" time we give the goodNode below before it expires.
# It is necessary to use this mechanism because the scheduler does not
# use mocktime -- so we must actually wait for it to expire.
FUZZ_TIME = 15.0


class WarnOnOutdatedTest(BitcoinTestFramework):
 
    def run_test(self):
        goodNode = self.nodes[0]
        outdatedNode = self.nodes[1]
        supressingNode = self.nodes[2]
        afterUpgradeNode = self.nodes[3]

        waitTime = FUZZ_TIME * 2
        # The following RPC calls show the warning.
        calls = ["getmininginfo", "getnetworkinfo", "getblockchaininfo"]

        for c in calls:
            assert WARNING_TEXT_SOON not in getattr(goodNode, c)()["warnings"]
            assert WARNING_TEXT_SOON in getattr(outdatedNode, c)()["warnings"]
            assert WARNING_TEXT_SOON not in getattr(supressingNode, c)()["warnings"]
            assert WARNING_TEXT_EXPIRED in getattr(afterUpgradeNode, c)()["warnings"]

        with goodNode.assert_debug_log(expected_msgs=[WARNING_TEXT_SOON], timeout=waitTime):
            # The call wait_until throws if the condition fails.
            self.log.info("Waiting up to {} seconds for warning to get triggered for node0"
                          .format(waitTime))

            for c in calls:
                wait_until(lambda: WARNING_TEXT_SOON in getattr(goodNode, c)()["warnings"],
                           timeout=waitTime)

            self.log.info("Warning text for node0 should now also appear in debug log")

        # Node 3 should show in the debug log that it's out of date.
        # It printed this message once on startup and will print it every hour.
        self.log.info("Checking node3 log file for text '{}'".format(WARNING_TEXT_EXPIRED))
        debug_log = os.path.join(afterUpgradeNode.datadir, self.chain, 'debug.log')
        with open(debug_log, encoding='utf-8') as dl:
            assert WARNING_TEXT_EXPIRED in dl.read()


if __name__ == '__main__':
    WarnOnOutdatedTest().main()
