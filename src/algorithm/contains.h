// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iterator>

namespace algo {

template <typename C, typename K>
// requires AssociativeContainer<C> && Regular<K>
inline
bool contains(C const& c, K const& x) {
    return c.find(x) != std::end(c);
}

} // namespace algo
