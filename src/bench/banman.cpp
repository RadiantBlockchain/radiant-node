// Copyright (c) 2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <bench/bench.h>

#include <banman.h>
#include <chainparams.h>
#include <config.h>
#include <crypto/siphash.h>
#include <fs.h>
#include <random.h>
#include <netaddress.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>
#include <vector>

// Generator for pseudo-random IPv4 addresses using a seed and Sip hash.
class AddrGen
{
    CSipHasher hasher{GetRand(std::numeric_limits<uint64_t>::max()), GetRand(std::numeric_limits<uint64_t>::max())};
    uint64_t seed = GetRand(std::numeric_limits<uint64_t>::max());
public:
    CNetAddr operator()()
    {
        hasher.Write(++seed);
        const uint64_t rawAddr = hasher.Finalize();
        hasher.Write(rawAddr);
        struct in_addr ipv4;
        std::memcpy(&ipv4, &rawAddr, std::min(sizeof(ipv4), sizeof(rawAddr)));
        return CNetAddr{ipv4};
    }
};


static void BanManAddressIsBanned(benchmark::State &state) {
    constexpr size_t nAddressesBanned = 65535, nAddressChk = 2000;
    SelectParams(CBaseChainParams::MAIN);
    const Config &config = GetConfig();
    const CChainParams params = config.GetChainParams();
    std::vector<CNetAddr> chkAddresses{nAddressChk};
    AddrGen gen;
    BanMan banman({}, params, nullptr, 60 * 60 * 24);
    for (auto & addr : chkAddresses)
        addr = gen();
    for (size_t i = 0; i < nAddressesBanned; ++i)
        banman.Ban(gen(), 0, false, false /* disable slow saves to disk */);

    size_t index = 0;
    while (state.KeepRunning()) {
        banman.IsBanned(chkAddresses[index++ % nAddressChk]);
    }
}

static void BanManAddressIsDiscouraged(benchmark::State &state) {
    constexpr size_t nAddressesBanned = BanMan::DiscourageFilterSize(), nAddressChk = 1000;
    SelectParams(CBaseChainParams::MAIN);
    const Config &config = GetConfig();
    const CChainParams params = config.GetChainParams();
    std::vector<CNetAddr> chkAddresses{nAddressChk};
    AddrGen gen;
    BanMan banman({}, params, nullptr, 60 * 60 * 24);
    for (auto & addr : chkAddresses)
        addr = gen();
    for (size_t i = 0; i < nAddressesBanned; ++i)
        banman.Discourage(gen());

    size_t index = 0;
    while (state.KeepRunning()) {
        banman.IsDiscouraged(chkAddresses[index++ % nAddressChk]);
    }
}

static void BanManAddressBan(benchmark::State &state) {
    constexpr size_t nAddressGen = 1000;
    SelectParams(CBaseChainParams::MAIN);
    const Config &config = GetConfig();
    const CChainParams params = config.GetChainParams();
    std::vector<CNetAddr> addresses{nAddressGen};
    AddrGen gen;
    BanMan banman({}, params, nullptr, 60 * 60 * 24);
    for (auto & addr : addresses)
        addr = gen();
    size_t banTime = 60, index = 0;
    while (state.KeepRunning()) {
        banman.Ban(addresses[index++ % nAddressGen], banTime++, false, false /* disable slow saves to disk */);
    }
}


BENCHMARK(BanManAddressIsBanned, 500);
BENCHMARK(BanManAddressIsDiscouraged, 20000000);
BENCHMARK(BanManAddressBan, 2400000);
