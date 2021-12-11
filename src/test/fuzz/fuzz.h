// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

const std::function<std::string(const char *)> G_TRANSLATION_FUN = nullptr;

void test_one_input(std::vector<uint8_t> buffer);
