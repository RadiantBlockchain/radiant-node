// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <crypto/siphash.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <cstdint>
#include <functional>

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
    SaltedUint256Hasher() noexcept {} // circumvent some libstdc++-11 bugs on Debian unstable
    size_t operator()(const uint256 &u) const noexcept {
        return static_cast<size_t>(SipHashUint256(k0(), k1(), u));
    }
};

struct SaltedTxIdHasher : protected SaltedUint256Hasher {
    SaltedTxIdHasher() noexcept {} // circumvent some libstdc++-11 bugs on Debian unstable
    size_t operator()(const TxId &u) const noexcept { return SaltedUint256Hasher::operator()(u); }
};

struct SaltedOutpointHasher : SaltedHasherBase {
    SaltedOutpointHasher() noexcept {} // circumvent some libstdc++-11 bugs on Debian unstable
    size_t operator()(const COutPoint &o) const noexcept {
        return static_cast<size_t>(SipHashUint256Extra(k0(), k1(), o.GetTxId(), o.GetN()));
    }
};

/// @class StdHashWrapper
/// Wraps std::hash<T> with a (superior) salted Sip-2-4 hasher
template <typename T>
class StdHashWrapper : public SaltedHasherBase {
    std::hash<T> wrappedHasher;
public:
    StdHashWrapper() noexcept {} // circumvent some libstdc++-11 bugs on Debian unstable
    size_t operator()(const T &t) const noexcept {
        const size_t val = wrappedHasher(t);
        if constexpr (sizeof(val) == sizeof(uint64_t))
            return static_cast<size_t>(CSipHasher(k0(), k1()).Write(static_cast<uint64_t>(val)).Finalize());
        else
            return static_cast<size_t>(CSipHasher(k0(), k1())
                                       .Write(reinterpret_cast<const uint8_t *>(&val), sizeof(val)).Finalize());
    }
};

/**
 * For types that internally store a byte array.
 */
struct ByteVectorHash : SaltedHasherBase {
    ByteVectorHash() noexcept {} // circumvent some libstdc++-11 bugs on Debian unstable
    size_t operator()(const std::vector<uint8_t> &input) const noexcept;
};
