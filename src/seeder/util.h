// Copyright (c) 2017-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SEEDER_UTIL_H
#define BITCOIN_SEEDER_UTIL_H

#include <chrono>
#include <thread>

#define BEGIN(a) ((char *)&(a))
#define END(a) ((char *)&((&(a))[1]))

static inline void Sleep(int nMilliSec) {
    std::this_thread::sleep_for(std::chrono::milliseconds{nMilliSec});
}

#endif // BITCOIN_SEEDER_UTIL_H
