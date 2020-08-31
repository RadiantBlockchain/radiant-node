// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <span.h>

#include <boost/algorithm/string.hpp>

#include <algorithm>
#include <array>
#include <cstring>
#include <string>
#include <vector>

[[nodiscard]] inline std::string TrimString(const std::string &str, const std::string &pattern = " \f\n\r\t\v") {
    std::string::size_type front = str.find_first_not_of(pattern);
    if (front == std::string::npos) {
        return std::string();
    }
    std::string::size_type end = str.find_last_not_of(pattern);
    return str.substr(front, end - front + 1);
}

/**
 * Join a list of items
 *
 * @param list       The list to join
 * @param separator  The separator
 * @param unary_op   Apply this operator to each item in the list
 */
template <typename T, typename UnaryOp>
std::string Join(const std::vector<T> &list, const std::string &separator, UnaryOp unary_op) {
    std::string ret;
    for (size_t i = 0; i < list.size(); ++i) {
        if (i > 0) ret += separator;
        ret += unary_op(list[i]);
    }
    return ret;
}

inline std::string Join(const std::vector<std::string> &list, const std::string &separator) {
    return Join(list, separator, [](const std::string &i) { return i; });
}

template<typename SequenceSequenceT, typename RangeT>
SequenceSequenceT& Split(SequenceSequenceT &Result, RangeT &&Input, const std::string &pattern = " \f\n\r\t\v", bool token_compress_on = false) {
    return boost::split(Result, Input, boost::is_any_of(pattern), 
                        token_compress_on ? boost::token_compress_on : boost::token_compress_off);
}

/**
 * Check if a string does not contain any embedded NUL (\0) characters
 */
[[nodiscard]] inline bool ValidAsCString(const std::string &str) noexcept {
    return str.find_first_of('\0') == std::string::npos;
}

/**
 * Check whether a container begins with the given prefix.
 */
template <typename T1>
[[nodiscard]] inline bool HasPrefix(const T1 &obj, const Span<const uint8_t> prefix) {
    return obj.size() >= prefix.size() && std::equal(prefix.begin(), prefix.end(), obj.begin());
}
