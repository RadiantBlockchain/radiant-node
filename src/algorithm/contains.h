// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ALGORITHM_CONTAINS_H_
#define BITCOIN_ALGORITHM_CONTAINS_H_

#include <iterator>

namespace algo {

template <typename C, typename K>
// requires AssociativeContainer<C> && Regular<K>
inline
bool contains(C const& c, K const& x) {
    return c.find(x) != std::end(c);
}

} // namespace algo

#endif // BITCOIN_ALGORITHM_CONTAINS_H_
