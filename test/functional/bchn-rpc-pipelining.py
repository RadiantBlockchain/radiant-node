#!/usr/bin/env python3
# Copyright (c) 2020 The Bitcoin Cash Node developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test RPC HTTP/1.1 pipelining."""

import urllib
import socket
import re

from test_framework.test_framework import BitcoinTestFramework
from test_framework.util import str_to_b64str, assert_equal


success_re = re.compile(r'HTTP\/1\.1 200')


def make_request(data: str, auth: str = None) -> bytes:
    lines = ["POST / HTTP/1.1"]

    if auth:
        lines.append(f'Authorization: Basic {str_to_b64str(auth)}')

    data_enc = data.encode('ascii')
    lines.append(f'Content-Length: {len(data_enc)}')
    lines.append('')
    lines.append(data)

    return '\n'.join(lines).encode('ascii')


class HTTPPipeliningTest(BitcoinTestFramework):
    def set_test_params(self):
        self.num_nodes = 1
        self.extra_args = [
            ["-rpcauth=rt:2bfb948eb35cc52418c64ed33829f67f$55049ebf3e1ddea997798cf4eb7d74ed57974468634bdb89f61bce7f2fbcaa9e"]] * self.num_nodes

    def run_test(self):
        url = urllib.parse.urlparse(self.nodes[0].url)

        authpair = "rt:8hTNHo3xJhZl-QJDjteo6YwJnAWLR3XcnpIWCMZnZk0="

        self.log.info('Connecting...')

        try:
            client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            # Disable Nagle algorithm so that each send makes a new packet
            client.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            client.connect((url.hostname, url.port))

            req = make_request(
                '{"method":"getblockchaininfo","params":[],"id":1}', authpair)

            pipelinedepth = 10

            # Send in a single packet

            mreq = bytearray()
            for i in range(0, pipelinedepth):
                mreq.extend(req)

            self.log.info(
                f'Sending {pipelinedepth} pipelined requests in a single packet...')
            client.send(mreq)

            self.log.info('Receiving responses...')
            recvbuf = bytearray()
            while True:
                recvbuf.extend(client.recv(4096))
                recvbufa = recvbuf.decode('ascii')
                responses = success_re.findall(recvbufa)
                if len(responses) == pipelinedepth:
                    break

            self.log.info(f'Received {len(responses)} responses...')
            assert_equal(len(responses), pipelinedepth)

            # Send in multiple packets

            self.log.info(
                f'Sending {pipelinedepth} pipelined requests in multiple packets...')
            for i in range(0, pipelinedepth):
                client.send(req)

            self.log.info('Receiving responses...')
            recvbuf = bytearray()
            while True:
                recvbuf.extend(client.recv(4096))
                recvbufa = recvbuf.decode('ascii')
                responses = success_re.findall(recvbufa)
                if len(responses) == pipelinedepth:
                    break

            self.log.info(f'Received {len(responses)} responses...')
            assert_equal(len(responses), pipelinedepth)
        finally:
            client.close()


if __name__ == '__main__':
    HTTPPipeliningTest().main()
