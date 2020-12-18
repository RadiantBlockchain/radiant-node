// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ATTRIBUTES_H
#define BITCOIN_ATTRIBUTES_H

/* This file is a place to put macros for decorating functions and variables.
 *
 * It is currently empty, however it used to be the place where the NODISCARD
 * macro once lived. After switching to C++17, we opted to instead retire the
 * macro in favor of the C++17 guaranteed-to-exist-always-on-every-compiler:
 * [[nodiscard]] attribute.
 *
 * However, this file is being kept in the codebase in case we need to someday
 * once again add attribute macros to the codebase, in which case this is where
 * they should live.
 */

#endif // BITCOIN_ATTRIBUTES_H
