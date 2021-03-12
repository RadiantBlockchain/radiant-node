// Copyright (c) 2018 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/saltedhashers.h>

#include <random.h>

SaltedHasherBase::SaltedHasherBase() noexcept
    : m_k0(GetRand64()), m_k1(GetRand64())
{}

size_t ByteVectorHash::operator()(const std::vector<uint8_t> &input) const noexcept {
    return static_cast<size_t>(CSipHasher(k0(), k1()).Write(input.data(), input.size()).Finalize());
}
