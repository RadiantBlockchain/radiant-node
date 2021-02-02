// Copyright (c) 2015 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <util/system.h>
#include <zmq/zmqabstractnotifier.h>

#include <cassert>

CZMQAbstractNotifier::~CZMQAbstractNotifier() {
    assert(!psocket);
}

bool CZMQAbstractNotifier::NotifyBlock(const CBlockIndex * /*CBlockIndex*/) {
    return true;
}

bool CZMQAbstractNotifier::NotifyTransaction(
    const CTransaction & /*transaction*/) {
    return true;
}

bool CZMQAbstractNotifier::NotifyDoubleSpend(const CTransaction & /*transaction*/) {
    return true;
}
