// Copyright (c) 2018 The Bitcoin Core developers
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.


#include <rpc/server.h>
#include <rpc/util.h>
#include <zmq/zmqabstractnotifier.h>
#include <zmq/zmqnotificationinterface.h>
#include <zmq/zmqrpc.h>

#include <univalue.h>

namespace {

UniValue getzmqnotifications(const Config &,
                             const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() != 0) {
        throw std::runtime_error(RPCHelpMan{
            "getzmqnotifications",
            "\nReturns information about the active ZeroMQ notifications.\n",
            {},
            RPCResult{
                "[\n"
                "  {                        (json object)\n"
                "    \"type\": \"pubhashtx\",   (string) Type of notification\n"
                "    \"address\": \"...\"       (string) Address of the publisher\n"
                "  },\n"
                "  ...\n"
                "]\n"},
            RPCExamples{HelpExampleCli("getzmqnotifications", "") +
                        HelpExampleRpc("getzmqnotifications", "")},
        }.ToStringWithResultsAndExamples());
    }

    UniValue::Array result;
    if (g_zmq_notification_interface != nullptr) {
        auto notifiers = g_zmq_notification_interface->GetActiveNotifiers();
        result.reserve(notifiers.size());
        for (const auto *n : notifiers) {
            UniValue::Object obj;
            obj.reserve(2);
            obj.emplace_back("type", n->GetType());
            obj.emplace_back("address", n->GetAddress());
            result.emplace_back(std::move(obj));
        }
    }

    return result;
}

// clang-format off
static const ContextFreeRPCCommand commands[] = {
    //  category          name                     actor (function)        argNames
    //  ----------------- ------------------------ ----------------------- ----------
    { "zmq",            "getzmqnotifications",   getzmqnotifications,    {} },
};
// clang-format on

} // anonymous namespace

void RegisterZMQRPCCommands(CRPCTable &t) {
    for (const auto &c : commands) {
        t.appendCommand(c.name, &c);
    }
}
