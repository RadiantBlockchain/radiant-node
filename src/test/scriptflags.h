// Copyright (c) 2017 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <string>

uint32_t ParseScriptFlags(const std::string &strFlags);
std::string FormatScriptFlags(uint32_t flags);
