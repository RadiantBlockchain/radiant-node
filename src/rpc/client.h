// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RPC_CLIENT_H
#define BITCOIN_RPC_CLIENT_H

#include <univalue.h>

/** Convert positional arguments to command-specific RPC representation */
UniValue::Array RPCConvertValues(const std::string &strMethod, const std::vector<std::string> &strParams);

/** Convert named arguments to command-specific RPC representation */
UniValue::Object RPCConvertNamedValues(const std::string &strMethod, const std::vector<std::string> &strParams);

#endif // BITCOIN_RPC_CLIENT_H
