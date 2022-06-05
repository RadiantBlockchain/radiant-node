#!/usr/bin/env python3
# Copyright (c) 2016-2019 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Encode and decode BASE58, P2PKH addresses."""

import unittest

from .script import hash256, hash160, CScript
from .util import hex_str_to_bytes

from test_framework.util import assert_equal


chars = '123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz'


def byte_to_base58(b, version):
    result = ''
    str = b.hex()
    str = chr(version).encode('latin-1').hex() + str
    checksum = hash256(hex_str_to_bytes(str)).hex()
    str += checksum[:8]
    value = int('0x' + str, 0)
    while value > 0:
        result = chars[value % 58] + result
        value //= 58
    while (str[:2] == '00'):
        result = chars[0] + result
        str = str[2:]
    return result


def base58_to_byte(s, verify_checksum=True):
    '''Converts a base58-encoded string to its data and version.

    If verify_checksum is True, then this function will
    raise AssertionError if the base58 checksum is invalid.

    >>> base58_to_byte('', verify_checksum=True)
    b''

    # 0x00759D6677091E973B9E9D99F19C68FBF43E3F05F95EABD8A1
    >>> base58_to_byte('1BitcoinEaterAddressDontSendf59kuE', verify_checksum=True)
    (b'u\\x9dfw\\t\\x1e\\x97;\\x9e\\x9d\\x99\\xf1\\x9ch\\xfb\\xf4>?\\x05\\xf9', 0)

    >>> base58_to_byte('miPp79eFw9rjKFoUtcrqNMqXWqNVmnzfug')
    (b'\\x1f\\x8e\\xa1p*{\\xd4\\x94\\x1b\\xca\\tA\\xb8R\\xc4\\xbb\\xfe\\xdb.\\x05', 111)

    >>> base58_to_byte('2cFupjhnEsSn59qHXstmK2ffpLv2', verify_checksum=False)
    (b'imply a long st', 115)

    >>> base58_to_byte("1NS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L", verify_checksum=False)
    (b'\\xeb\\x15#\\x1d\\xfc\\xeb`\\x92X\\x86\\xb6}\\x06R\\x99\\x92Y\\x15\\xae\\xb1', 0)

    0x516b6fcd0f
    >>> base58_to_byte("ABnLTmg", verify_checksum=False)
    (b'', 81)

    >>> base58_to_byte( "3SEo3LWLoPntC", verify_checksum=False)
    (b'O\\x89\\x00\\x1e', 191)

    >>> base58_to_byte("EJDM8drfXA6uyA", verify_checksum=False)
    (b'\\xac\\x89\\xca\\xd99', 236)

    >>> base58_to_byte("1111111111", verify_checksum=False)
    (b'\\x00\\x00\\x00\\x00\\x00', 0)

    >>> base58_to_byte("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz", verify_checksum=False)
    (b'\\x01\\x11\\xd3\\x8e_\\xc9\\x07\\x1f\\xfc\\xd2\\x0bJv<\\xc9\\xaeO%+\\xb4\\xe4\\x8f\\xd6j\\x83^%*\\xda\\x93\\xffH\\rm\\xd4=\\xc6*', 0)

    >>> base58_to_byte("1cWB5HCBdLjAuqGGReWE3R3CguuwSjw6RHn39s2yuDRTS5NsBgNiFpWgAnEx6VQi8csexkgYw3mdYrMHr8x9i7aEwP8kZ7vccXWqKDvGv3u1GxFKPuAkn8JCPPGDMf3vMMnbzm6Nh9zh1gcNsMvH3ZNLmP5fSG6DGbbi2tuwMWPthr4boWwCxf7ewSgNQeacyozhKDDQQ1qL5fQFUW52QKUZDZ5fw3KXNQJMcNTcaB723LchjeKun7MuGW5qyCBZYzA1KjofN1gYBV3NqyhQJ3Ns746GNuf9N2pQPmHz4xpnSrrfCvy6TVVz5d4PdrjeshsWQwpZsZGzvbdAdN8MKV5QsBDY", verify_checksum=False)
    (b'\\x01\\x02\\x03\\x04\\x05\\x06\\x07\\x08\\t\\n\\x0b\\x0c\\r\\x0e\\x0f\\x10\\x11\\x12\\x13\\x14\\x15\\x16\\x17\\x18\\x19\\x1a\\x1b\\x1c\\x1d\\x1e\\x1f !"#$%&\\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\\\]^_`abcdefghijklmnopqrstuvwxyz{|}~\\x7f\\x80\\x81\\x82\\x83\\x84\\x85\\x86\\x87\\x88\\x89\\x8a\\x8b\\x8c\\x8d\\x8e\\x8f\\x90\\x91\\x92\\x93\\x94\\x95\\x96\\x97\\x98\\x99\\x9a\\x9b\\x9c\\x9d\\x9e\\x9f\\xa0\\xa1\\xa2\\xa3\\xa4\\xa5\\xa6\\xa7\\xa8\\xa9\\xaa\\xab\\xac\\xad\\xae\\xaf\\xb0\\xb1\\xb2\\xb3\\xb4\\xb5\\xb6\\xb7\\xb8\\xb9\\xba\\xbb\\xbc\\xbd\\xbe\\xbf\\xc0\\xc1\\xc2\\xc3\\xc4\\xc5\\xc6\\xc7\\xc8\\xc9\\xca\\xcb\\xcc\\xcd\\xce\\xcf\\xd0\\xd1\\xd2\\xd3\\xd4\\xd5\\xd6\\xd7\\xd8\\xd9\\xda\\xdb\\xdc\\xdd\\xde\\xdf\\xe0\\xe1\\xe2\\xe3\\xe4\\xe5\\xe6\\xe7\\xe8\\xe9\\xea\\xeb\\xec\\xed\\xee\\xef\\xf0\\xf1\\xf2\\xf3\\xf4\\xf5\\xf6\\xf7\\xf8\\xf9\\xfa\\xfb', 0)
    '''
    if not s:
        return b''
    n = 0
    for c in s:
        n *= 58
        assert c in chars
        digit = chars.index(c)
        n += digit
    h = '{:x}'.format(n)
    if len(h) % 2:
        h = '0' + h
    res = n.to_bytes((n.bit_length() + 7) // 8, 'big')
    pad = 0
    for c in s:
        if c == chars[0]:
            pad += 1
        else:
            break
    res = b'\x00' * pad + res
    if verify_checksum:
        assert_equal(hash256(res[:-4])[:4], res[-4:])

    return res[1:-4], int(res[0])


def keyhash_to_p2pkh(hash, main=False):
    assert (len(hash) == 20)
    version = 0 if main else 111
    return byte_to_base58(hash, version)

def key_to_p2pkh(key, main=False):
    key = check_key(key)
    return keyhash_to_p2pkh(hash160(key), main)

def check_key(key):
    if (isinstance(key, str)):
        key = hex_str_to_bytes(key)  # Assuming this is hex string
    if (isinstance(key, bytes) and (len(key) == 33 or len(key) == 65)):
        return key
    assert False


def check_script(script):
    if (isinstance(script, str)):
        script = hex_str_to_bytes(script)  # Assuming this is hex string
    if (isinstance(script, bytes) or isinstance(script, CScript)):
        return script
    assert False


class TestFrameworkScript(unittest.TestCase):
    def test_base58encodedecode(self):
        def check_base58(data, version):
            self.assertEqual(base58_to_byte(byte_to_base58(data, version)), (data, version))

        check_base58(bytes.fromhex('1f8ea1702a7bd4941bca0941b852c4bbfedb2e05'), 111)
        check_base58(bytes.fromhex('3a0b05f4d7f66c3ba7009f453530296c845cc9cf'), 111)
        check_base58(bytes.fromhex('41c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('0041c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('000041c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('00000041c1eaf111802559bad61b60d62b1f897c63928a'), 111)
        check_base58(bytes.fromhex('1f8ea1702a7bd4941bca0941b852c4bbfedb2e05'), 0)
        check_base58(bytes.fromhex('3a0b05f4d7f66c3ba7009f453530296c845cc9cf'), 0)
        check_base58(bytes.fromhex('41c1eaf111802559bad61b60d62b1f897c63928a'), 0)
        check_base58(bytes.fromhex('0041c1eaf111802559bad61b60d62b1f897c63928a'), 0)
        check_base58(bytes.fromhex('000041c1eaf111802559bad61b60d62b1f897c63928a'), 0)
        check_base58(bytes.fromhex('00000041c1eaf111802559bad61b60d62b1f897c63928a'), 0)


if __name__ == '__main__':
    import doctest
    doctest.testmod()
