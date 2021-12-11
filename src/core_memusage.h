// Copyright (c) 2015-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memusage.h>
#include <primitives/block.h>
#include <primitives/transaction.h>

inline size_t RecursiveDynamicUsage(const CScript &script) {
    return memusage::DynamicUsage(*static_cast<const CScriptBase *>(&script));
}

inline constexpr size_t RecursiveDynamicUsage(const COutPoint &) {
    return 0;
}

inline size_t RecursiveDynamicUsage(const CTxIn &in) {
    return RecursiveDynamicUsage(in.scriptSig) +
           RecursiveDynamicUsage(in.prevout);
}

inline size_t RecursiveDynamicUsage(const CTxOut &out) {
    return RecursiveDynamicUsage(out.scriptPubKey);
}

inline size_t RecursiveDynamicUsage(const CTransaction &tx) {
    size_t mem =
        memusage::DynamicUsage(tx.vin) + memusage::DynamicUsage(tx.vout);
    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin();
         it != tx.vin.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    for (std::vector<CTxOut>::const_iterator it = tx.vout.begin();
         it != tx.vout.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    return mem;
}

inline size_t RecursiveDynamicUsage(const CMutableTransaction &tx) {
    size_t mem =
        memusage::DynamicUsage(tx.vin) + memusage::DynamicUsage(tx.vout);
    for (std::vector<CTxIn>::const_iterator it = tx.vin.begin();
         it != tx.vin.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    for (std::vector<CTxOut>::const_iterator it = tx.vout.begin();
         it != tx.vout.end(); it++) {
        mem += RecursiveDynamicUsage(*it);
    }
    return mem;
}

template <typename X>
inline size_t RecursiveDynamicUsage(const std::shared_ptr<X> &p) {
    return p ? memusage::DynamicUsage(p) + RecursiveDynamicUsage(*p) : 0;
}
