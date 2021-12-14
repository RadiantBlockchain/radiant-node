// Copyright (c) 2017 Amaury SÉCHET
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <feerate.h>
#include <policy/policy.h>

#include <boost/noncopyable.hpp>

#include <cstdint>
#include <memory>
#include <set>
#include <string>

/** Default for -usecashaddr */
static constexpr bool DEFAULT_USE_CASHADDR = true;

class CChainParams;

class Config : public boost::noncopyable {
public:
    /** The largest block size this node will accept. */
    virtual bool SetExcessiveBlockSize(uint64_t maxBlockSize) = 0;
    virtual uint64_t GetExcessiveBlockSize() const = 0;
    /** The largest block size this node will generate (mine).
        Returns false if `blockSize` exceeds GetExcessiveBlockSize(). */
    virtual bool SetGeneratedBlockSize(uint64_t blockSize) = 0;
    virtual uint64_t GetGeneratedBlockSize() const = 0;
    /** The maximum amount of RAM to be used in the mempool before TrimToSize is called. */
    virtual void SetMaxMemPoolSize(uint64_t maxMemPoolSize) = 0;
    virtual uint64_t GetMaxMemPoolSize() const = 0;
    virtual bool SetInvBroadcastRate(uint64_t rate) = 0;
    virtual uint64_t GetInvBroadcastRate() const = 0;
    virtual bool SetInvBroadcastInterval(uint64_t interval) = 0;
    virtual uint64_t GetInvBroadcastInterval() const = 0;
    virtual const CChainParams &GetChainParams() const = 0;
    virtual void SetCashAddrEncoding(bool) = 0;
    virtual bool UseCashAddrEncoding() const = 0;

    virtual void SetExcessUTXOCharge(Amount amt) = 0;
    virtual Amount GetExcessUTXOCharge() const = 0;

    virtual void SetRejectSubVersions(const std::set<std::string> &reject) = 0;
    virtual const std::set<std::string> & GetRejectSubVersions() const = 0;

    virtual void SetGBTCheckValidity(bool) = 0;
    virtual bool GetGBTCheckValidity() const = 0;
};

class GlobalConfig final : public Config {
public:
    GlobalConfig();
    //! Note: `maxBlockSize` must not be smaller than 1MB and cannot exceed 2GB
    bool SetExcessiveBlockSize(uint64_t maxBlockSize) override;
    uint64_t GetExcessiveBlockSize() const override;
    bool SetGeneratedBlockSize(uint64_t blockSize) override;
    uint64_t GetGeneratedBlockSize() const override;
    void SetMaxMemPoolSize(uint64_t maxMemPoolSize) override { nMaxMemPoolSize = maxMemPoolSize; }
    uint64_t GetMaxMemPoolSize() const override { return nMaxMemPoolSize; }
    //! Note: `rate` may not exceed MAX_INV_BROADCAST_RATE (1 million)
    bool SetInvBroadcastRate(uint64_t rate) override;
    uint64_t GetInvBroadcastRate() const override { return nInvBroadcastRate; }
    //! Note: `interval` may not exceed MAX_INV_BROADCAST_INTERVAL (1 million)
    bool SetInvBroadcastInterval(uint64_t interval) override;
    uint64_t GetInvBroadcastInterval() const override { return nInvBroadcastInterval; }
    const CChainParams &GetChainParams() const override;
    void SetCashAddrEncoding(bool) override;
    bool UseCashAddrEncoding() const override;

    void SetExcessUTXOCharge(Amount) override;
    Amount GetExcessUTXOCharge() const override;

    void SetRejectSubVersions(const std::set<std::string> &reject) override { rejectSubVersions = reject; }
    const std::set<std::string> & GetRejectSubVersions() const override { return rejectSubVersions; }

    void SetGBTCheckValidity(bool b) override { gbtCheckValidity = b; }
    bool GetGBTCheckValidity() const override { return gbtCheckValidity; }

private:
    bool useCashAddr;
    bool gbtCheckValidity;
    Amount excessUTXOCharge;
    uint64_t nInvBroadcastRate;
    uint64_t nInvBroadcastInterval;

    /** The largest block size this node will accept. */
    uint64_t nExcessiveBlockSize;

    /** The largest block size this node will generate. */
    uint64_t nGeneratedBlockSize;

    /** The maximum amount of RAM to be used in the mempool before TrimToSize is called. */
    uint64_t nMaxMemPoolSize;

    std::set<std::string> rejectSubVersions;
};

// Dummy for subclassing in unittests
class DummyConfig : public Config {
public:
    DummyConfig();
    DummyConfig(const std::string &net);
    DummyConfig(std::unique_ptr<CChainParams> chainParamsIn);
    bool SetExcessiveBlockSize(uint64_t) override { return false; }
    uint64_t GetExcessiveBlockSize() const override { return 0; }
    bool SetGeneratedBlockSize(uint64_t) override { return false; }
    uint64_t GetGeneratedBlockSize() const override { return 0; }
    void SetMaxMemPoolSize(uint64_t) override {}
    uint64_t GetMaxMemPoolSize() const override {return 0; }
    bool SetInvBroadcastRate(uint64_t) override { return false; }
    uint64_t GetInvBroadcastRate() const override { return 0; }
    bool SetInvBroadcastInterval(uint64_t) override { return false; }
    uint64_t GetInvBroadcastInterval() const override {return 0; }

    void SetChainParams(const std::string &net);
    const CChainParams &GetChainParams() const override { return *chainParams; }

    void SetCashAddrEncoding(bool) override {}
    bool UseCashAddrEncoding() const override { return false; }

    void SetExcessUTXOCharge(Amount) override {}
    Amount GetExcessUTXOCharge() const override { return Amount::zero(); }

    void SetRejectSubVersions(const std::set<std::string> &) override {}
    const std::set<std::string> & GetRejectSubVersions() const override {
        static const std::set<std::string> dummy;
        return dummy;
    }

    void SetGBTCheckValidity(bool) override {}
    bool GetGBTCheckValidity() const override { return false; }

private:
    std::unique_ptr<CChainParams> chainParams;
};

// Temporary woraround.
const Config &GetConfig();
