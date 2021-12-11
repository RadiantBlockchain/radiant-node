// Copyright (c) 2017-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <algorithm>
#include <chrono>
#include <thread>

inline void Sleep(int nMilliSec) {
    std::this_thread::sleep_for(std::chrono::milliseconds{std::max(0, nMilliSec)});
}
