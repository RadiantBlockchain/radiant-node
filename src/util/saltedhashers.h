// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_UTIL_SALTEDHASHERS_H
#define BITCOIN_UTIL_SALTEDHASHERS_H

#include <crypto/siphash.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <cstdint>

/**
 *  Hashers for common types for use with e.g. std::unordered_map.
 *  Internally they all use SipHash-2-4 seeded with a per-instance random salt.
 *  Note: The hashers below or any new hashers added should always return size_t.
 */

class SaltedHasherBase {
    uint64_t m_k0, m_k1; ///< salt
protected:
    SaltedHasherBase() noexcept;
    uint64_t k0() const noexcept { return m_k0; }
    uint64_t k1() const noexcept { return m_k1; }
};

struct SaltedUint256Hasher : SaltedHasherBase {
    size_t operator()(const uint256 &u) const noexcept {
        return static_cast<size_t>(SipHashUint256(k0(), k1(), u));
    }
};

struct SaltedTxIdHasher : protected SaltedUint256Hasher {
    size_t operator()(const TxId &u) const noexcept { return SaltedUint256Hasher::operator()(u); }
};

struct SaltedOutpointHasher : SaltedHasherBase {
    size_t operator()(const COutPoint &o) const noexcept {
        return static_cast<size_t>(SipHashUint256Extra(k0(), k1(), o.GetTxId(), o.GetN()));
    }
};

/**
 * For types that internally store a byte array.
 */
struct ByteVectorHash : SaltedHasherBase {
    size_t operator()(const std::vector<uint8_t> &input) const noexcept;
};

#endif
