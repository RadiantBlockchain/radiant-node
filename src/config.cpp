// Copyright (c) 2017-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <config.h>

#include <chainparams.h>
#include <consensus/consensus.h> // DEFAULT_EXCESSIVE_BLOCK_SIZE

#include <algorithm>

GlobalConfig::GlobalConfig()
    : useCashAddr(DEFAULT_USE_CASHADDR), nExcessiveBlockSize(DEFAULT_EXCESSIVE_BLOCK_SIZE),
      // NB: The generated block size is normally set in init.cpp to use chain-specific
      //     defaults which are often smaller than the DEFAULT_EXCESSIVE_BLOCK_SIZE.
      nGeneratedBlockSize(DEFAULT_EXCESSIVE_BLOCK_SIZE),
      nMaxMemPoolSize(DEFAULT_EXCESSIVE_BLOCK_SIZE * DEFAULT_MAX_MEMPOOL_SIZE_PER_MB) {}

bool GlobalConfig::SetExcessiveBlockSize(uint64_t blockSize) {
    // Do not allow maxBlockSize to be set below historic 1MB limit
    // It cannot be equal either because of the "must be big" UAHF rule.
    if (blockSize <= LEGACY_MAX_BLOCK_SIZE) {
        return false;
    }

    nExcessiveBlockSize = blockSize;

    // Maintain invariant: ensure that nGeneratedBlockSize <= nExcessiveBlockSize
    nGeneratedBlockSize = std::min(nExcessiveBlockSize, nGeneratedBlockSize);

    return true;
}

uint64_t GlobalConfig::GetExcessiveBlockSize() const {
    return nExcessiveBlockSize;
}

bool GlobalConfig::SetGeneratedBlockSize(uint64_t blockSize) {
    // Do not allow generated blocks to exceed the size of blocks we accept.
    if (blockSize > GetExcessiveBlockSize()) {
        return false;
    }

    nGeneratedBlockSize = blockSize;
    return true;
}

uint64_t GlobalConfig::GetGeneratedBlockSize() const {
    return nGeneratedBlockSize;
}

const CChainParams &GlobalConfig::GetChainParams() const {
    return Params();
}

static GlobalConfig gConfig;

const Config &GetConfig() {
    return gConfig;
}

void GlobalConfig::SetCashAddrEncoding(bool c) {
    useCashAddr = c;
}
bool GlobalConfig::UseCashAddrEncoding() const {
    return useCashAddr;
}

DummyConfig::DummyConfig()
    : chainParams(CreateChainParams(CBaseChainParams::REGTEST)) {}

DummyConfig::DummyConfig(const std::string &net)
    : chainParams(CreateChainParams(net)) {}

DummyConfig::DummyConfig(std::unique_ptr<CChainParams> chainParamsIn)
    : chainParams(std::move(chainParamsIn)) {}

void DummyConfig::SetChainParams(const std::string &net) {
    chainParams = CreateChainParams(net);
}

void GlobalConfig::SetExcessUTXOCharge(Amount fee) {
    excessUTXOCharge = fee;
}

Amount GlobalConfig::GetExcessUTXOCharge() const {
    return excessUTXOCharge;
}
