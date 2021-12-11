// Copyright (c) 2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <vector>

enum class ScriptError;

bool DecodeBitfield(const std::vector<uint8_t> &vch, unsigned size,
                    uint32_t &bitfield, ScriptError *serror);
