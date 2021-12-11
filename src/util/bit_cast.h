// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <cstring>
#include <type_traits>


/**
 *  bit_cast - similar to C++20's std::bit_cast. Allows for non-UB
 *  reinterpret_cast of two non-pointer-interconvertible types.
 */
template <class To, class From, bool unsafe = false>
std::enable_if_t<
    !std::is_pointer_v<From> && !std::is_pointer_v<To>
    && (unsafe || sizeof(To) <= sizeof(From))
    && std::is_trivially_copyable_v<From> && std::is_trivially_copyable_v<To>,
To> bit_cast(const From &src) noexcept
{
    static_assert(std::is_trivially_constructible_v<To>,
        "This implementation additionally requires destination type to be trivially constructible");

    To dst;
    std::memcpy(&dst, &src, sizeof(To));
    return dst;
}

template <class To, class From>
To bit_cast_unsafe(const From &src) { return bit_cast<To, From, true>(src); }
