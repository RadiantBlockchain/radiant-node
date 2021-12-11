// Copyright (c) 2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>
#include <vector>

namespace interfaces {
class Chain;
class ChainClient;
} // namespace interfaces

//! Pointers to interfaces used during init and destroyed on shutdown.
struct NodeContext {
    std::unique_ptr<interfaces::Chain> chain;
    std::vector<std::unique_ptr<interfaces::ChainClient>> chain_clients;
};
