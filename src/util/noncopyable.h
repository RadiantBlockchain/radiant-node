// Copyright (c) 2022 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

/// Non-copyable base class with deleted copy c'tor and copy assignment operator
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(NonCopyable const&) = delete;
    NonCopyable &operator=(NonCopyable const&) = delete;
};
