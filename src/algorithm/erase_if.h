// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iterator>

#include <algorithm/type_attributes.h>

namespace algo {

template <typename C, typename P>
// requires AssociativeContainer<C> && UnaryPredicate<P>
inline
attr::SizeType<C> erase_if(C& c, P pred) {
    auto const old_size = std::size(c);
    auto f = std::begin(c);
    auto const l = std::end(c);
    while (f != l) {
        if (pred(*f)) {
            f = c.erase(f);
        } else {
            ++f;
        }
    }
    return old_size - std::size(c);
}

} // namespace algo
