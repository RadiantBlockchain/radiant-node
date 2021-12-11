// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2015 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/**
 * Money parsing/formatting utilities.
 */
#pragma once

#include <amount.h>
#include <attributes.h>

#include <cstdint>
#include <string>

/**
 * Do not use these functions to represent or parse monetary amounts to or from
 * JSON but use AmountFromValue and ValueFromAmount for that.
 */
std::string FormatMoney(const Amount n);
[[nodiscard]] bool ParseMoney(const std::string &str, Amount &nRet);
[[nodiscard]] bool ParseMoney(const char *pszIn, Amount &nRet);
