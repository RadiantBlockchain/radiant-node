#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test the indexdir option.
"""

import os
import shutil

from test_framework.test_framework import BitcoinTestFramework, initialize_datadir
from test_framework.test_node import ErrorMatch


class IndexdirTest(BitcoinTestFramework):
    def set_test_params(self):
        self.setup_clean_chain = True
        self.num_nodes = 1

    def run_test(self):
        self.stop_node(0)
        shutil.rmtree(self.nodes[0].datadir)
        initialize_datadir(self.options.tmpdir, 0, self.chain)
        self.log.info("Starting with invalid indexdir ...")
        protected_path = os.path.join(self.options.tmpdir, "protected")
        open(protected_path, 'a', encoding='utf-8').close()
        self.nodes[0].assert_start_raises_init_error(
            ["-indexdir=" + protected_path],
            'Error: Error creating index directory: ',
            match=ErrorMatch.PARTIAL_REGEX)
        self.log.info("Starting with valid indexdir ...")
        indexdir_path = os.path.join(self.options.tmpdir, "foo", "test")
        self.start_node(0, ["-indexdir=" + indexdir_path])
        self.log.info("mining blocks..")
        self.generatetoaddress(self.nodes[0],
                               10, self.nodes[0].get_deterministic_priv_key().address)
        assert os.path.isdir(os.path.join(indexdir_path, self.chain, "index"))
        assert os.path.isfile(os.path.join(indexdir_path, self.chain, "index", "CURRENT"))
        self.stop_node(0)
        shutil.rmtree(self.nodes[0].datadir)
        initialize_datadir(self.options.tmpdir, 0, self.chain)
        self.log.info("Starting with default indexdir ...")
        self.start_node(0)
        self.log.info("mining blocks..")
        self.generatetoaddress(self.nodes[0],
                               10, self.nodes[0].get_deterministic_priv_key().address)
        assert os.path.isdir(os.path.join(self.nodes[0].datadir, self.chain, "blocks", "index"))
        assert os.path.isfile(os.path.join(self.nodes[0].datadir, self.chain, "blocks", "index", "CURRENT"))


if __name__ == '__main__':
    IndexdirTest().main()
