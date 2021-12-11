// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <memory>
#include <string>

class CBlockIndex;
class CTransaction;
class CZMQAbstractNotifier;

using CZMQNotifierFactory = std::unique_ptr<CZMQAbstractNotifier> (*)();

class CZMQAbstractNotifier {
public:
    CZMQAbstractNotifier() : psocket(nullptr) {}
    virtual ~CZMQAbstractNotifier();

    template <typename T>
    static std::unique_ptr<CZMQAbstractNotifier> Create() {
        return std::make_unique<T>();
    }

    std::string GetType() const { return type; }
    void SetType(const std::string &t) { type = t; }
    std::string GetAddress() const { return address; }
    void SetAddress(const std::string &a) { address = a; }

    virtual bool Initialize(void *pcontext) = 0;
    virtual void Shutdown() = 0;

    virtual bool NotifyBlock(const CBlockIndex *pindex);
    virtual bool NotifyTransaction(const CTransaction &transaction);
    virtual bool NotifyDoubleSpend(const CTransaction &transaction);

protected:
    void *psocket;
    std::string type;
    std::string address;
};
