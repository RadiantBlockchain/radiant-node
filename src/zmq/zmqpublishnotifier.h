// Copyright (c) 2015-2016 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <zmq/zmqabstractnotifier.h>

class CBlockIndex;

class CZMQAbstractPublishNotifier : public CZMQAbstractNotifier {
private:
    //! upcounting per message sequence number
    uint32_t nSequence;

public:
    /* send zmq multipart message
       parts:
          * command
          * data
          * message sequence number
    */
    bool SendZmqMessage(const char *command, const void *data, size_t size);

    bool Initialize(void *pcontext) override;
    void Shutdown() override;
};

class CZMQPublishHashBlockNotifier : public CZMQAbstractPublishNotifier {
public:
    bool NotifyBlock(const CBlockIndex *pindex) override;
};

class CZMQPublishHashTransactionNotifier : public CZMQAbstractPublishNotifier {
public:
    bool NotifyTransaction(const CTransaction &transaction) override;
};

class CZMQPublishRawBlockNotifier : public CZMQAbstractPublishNotifier {
public:
    bool NotifyBlock(const CBlockIndex *pindex) override;
};

class CZMQPublishRawTransactionNotifier : public CZMQAbstractPublishNotifier {
public:
    bool NotifyTransaction(const CTransaction &transaction) override;
};


class CZMQPublishHashDoubleSpendNotifier : public CZMQAbstractPublishNotifier {
public:
    bool NotifyDoubleSpend(const CTransaction &transaction) override;
};

class CZMQPublishRawDoubleSpendNotifier : public CZMQAbstractPublishNotifier {
public:
    bool NotifyDoubleSpend(const CTransaction &transaction) override;
};
