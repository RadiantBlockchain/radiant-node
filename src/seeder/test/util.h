// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <type_traits>

template <typename E>
constexpr typename std::underlying_type<E>::type to_integral(E e) {
    return static_cast<typename std::underlying_type<E>::type>(e);
}
