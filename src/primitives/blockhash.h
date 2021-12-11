// Copyright (c) 2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <uint256.h>

/**
 * A BlockHash is a unqiue identifier for a block.
 */
struct BlockHash : public uint256 {
    explicit constexpr BlockHash() noexcept : uint256() {}
    explicit constexpr BlockHash(const uint256 &b) noexcept : uint256(b) {}

    static BlockHash fromHex(const std::string &str) noexcept {
        return BlockHash(uint256S(str));
    }
};
