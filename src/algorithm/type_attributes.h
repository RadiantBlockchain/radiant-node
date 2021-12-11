// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <iterator>

namespace attr {

template <typename C>
// requires Container<C>
using SizeType = typename C::size_type;

} // namespace attr
