// Copyright (c) 2017-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#include <cashaddrenc.h>

#include <cashaddr.h>
#include <chainparams.h>
#include <pubkey.h>
#include <script/script.h>
#include <util/strencodings.h>
#include <key_io.h> // Include key_io for legacy address encoding/decoding

#include <boost/variant/static_visitor.hpp>

#include <algorithm>

namespace {

// Convert the data part to a 5 bit representation.
template <class T>
std::vector<uint8_t> PackAddrData(const T &id, uint8_t type) {
    (void)id; // Unused in legacy address support
    (void)type; // Unused in legacy address support
    return {};
}

// Implements encoding of CTxDestination using legacy address.
class CashAddrEncoder : public boost::static_visitor<std::string> {
public:
    CashAddrEncoder(const CChainParams &p) : params(p) {}

    std::string operator()(const CKeyID &id) const {
        return EncodeLegacyAddr(id, params);
    }

    std::string operator()(const CScriptID &id) const {
        return EncodeLegacyAddr(id, params);
    }

    std::string operator()(const CNoDestination &) const { return ""; }

private:
    const CChainParams &params;
};

} // namespace

std::string EncodeCashAddr(const CTxDestination &dst, const CChainParams &params) {
    return boost::apply_visitor(CashAddrEncoder(params), dst);
}

std::string EncodeCashAddr(const std::string &prefix, const CashAddrContent &content) {
    (void)prefix; // Unused
    (void)content; // Unused
    return "";
}

CTxDestination DecodeCashAddr(const std::string &addr, const CChainParams &params) {
    return DecodeLegacyAddr(addr, params);
}

CashAddrContent DecodeCashAddrContent(const std::string &addr, const std::string &expectedPrefix) {
    (void)addr; // Unused
    (void)expectedPrefix; // Unused
    return {};
}

CTxDestination DecodeCashAddrDestination(const CashAddrContent &content) {
    (void)content; // Unused
    return CNoDestination{};
}

// PackCashAddrContent allows for testing PackAddrData in unittests due to
// template definitions.
std::vector<uint8_t> PackCashAddrContent(const CashAddrContent &content) {
    return PackAddrData(content.hash, content.type);
}
