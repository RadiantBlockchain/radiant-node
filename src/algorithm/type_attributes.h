// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ALGORITHM_TYPE_ATTRIBUTES_H_
#define BITCOIN_ALGORITHM_TYPE_ATTRIBUTES_H_

#include <iterator>

namespace attr {

template <typename C>
// requires Container<C>
using SizeType = typename C::size_type;

} // namespace attr

#endif // BITCOIN_ALGORITHM_TYPE_ATTRIBUTES_H_
