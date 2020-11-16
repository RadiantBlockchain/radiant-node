// Copyright (c) 2017 Amaury SÉCHET
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONFIG_H
#define BITCOIN_CONFIG_H

#include <amount.h>
#include <feerate.h>
#include <policy/policy.h>

#include <boost/noncopyable.hpp>

#include <cstdint>
#include <memory>
#include <string>

/** Default for -usecashaddr */
static constexpr bool DEFAULT_USE_CASHADDR = true;

class CChainParams;

class Config : public boost::noncopyable {
public:
    virtual bool SetMaxBlockSize(uint64_t maxBlockSize) = 0;
    virtual uint64_t GetMaxBlockSize() const = 0;
    virtual void SetMaxMemPoolSize(uint64_t maxMemPoolSize) = 0;
    virtual uint64_t GetMaxMemPoolSize() const = 0;
    virtual void SetInvBroadcastRate(uint64_t rate) = 0;
    virtual uint64_t GetInvBroadcastRate() const = 0;
    virtual void SetInvBroadcastInterval(uint64_t interval) = 0;
    virtual uint64_t GetInvBroadcastInterval() const = 0;
    virtual const CChainParams &GetChainParams() const = 0;
    virtual void SetCashAddrEncoding(bool) = 0;
    virtual bool UseCashAddrEncoding() const = 0;

    virtual void SetExcessUTXOCharge(Amount amt) = 0;
    virtual Amount GetExcessUTXOCharge() const = 0;
};

class GlobalConfig final : public Config {
public:
    GlobalConfig();
    bool SetMaxBlockSize(uint64_t maxBlockSize) override;
    uint64_t GetMaxBlockSize() const override;
    void SetMaxMemPoolSize(uint64_t maxMemPoolSize) override { nMaxMemPoolSize = maxMemPoolSize; }
    uint64_t GetMaxMemPoolSize() const override { return nMaxMemPoolSize; }
    void SetInvBroadcastRate(uint64_t rate) override { nInvBroadcastRate = rate; }
    uint64_t GetInvBroadcastRate() const override { return nInvBroadcastRate; }
    void SetInvBroadcastInterval(uint64_t interval) override { nInvBroadcastInterval = interval; }
    uint64_t GetInvBroadcastInterval() const override { return nInvBroadcastInterval; }
    const CChainParams &GetChainParams() const override;
    void SetCashAddrEncoding(bool) override;
    bool UseCashAddrEncoding() const override;

    void SetExcessUTXOCharge(Amount) override;
    Amount GetExcessUTXOCharge() const override;

private:
    bool useCashAddr;
    Amount excessUTXOCharge;
    uint64_t nInvBroadcastRate;
    uint64_t nInvBroadcastInterval;

    /** The largest block size this node will accept. */
    uint64_t nMaxBlockSize;

    /** The maximum amount of RAM to be used in the mempool before TrimToSize is called. */
    uint64_t nMaxMemPoolSize;
};

// Dummy for subclassing in unittests
class DummyConfig : public Config {
public:
    DummyConfig();
    DummyConfig(const std::string &net);
    DummyConfig(std::unique_ptr<CChainParams> chainParamsIn);
    bool SetMaxBlockSize(uint64_t) override { return false; }
    uint64_t GetMaxBlockSize() const override { return 0; }
    void SetMaxMemPoolSize(uint64_t) override {}
    uint64_t GetMaxMemPoolSize() const override {return 0; }
    void SetInvBroadcastRate(uint64_t) override {}
    uint64_t GetInvBroadcastRate() const override { return 0; }
    void SetInvBroadcastInterval(uint64_t) override {}
    uint64_t GetInvBroadcastInterval() const override {return 0; }

    void SetChainParams(const std::string &net);
    const CChainParams &GetChainParams() const override { return *chainParams; }

    void SetCashAddrEncoding(bool) override {}
    bool UseCashAddrEncoding() const override { return false; }

    void SetExcessUTXOCharge(Amount amt) override {}
    Amount GetExcessUTXOCharge() const override { return Amount::zero(); }

private:
    std::unique_ptr<CChainParams> chainParams;
};

// Temporary woraround.
const Config &GetConfig();

#endif // BITCOIN_CONFIG_H
