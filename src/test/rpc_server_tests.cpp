// Copyright (c) 2018-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/jsonrpcrequest.h>
#include <rpc/server.h>

#include <chainparams.h>
#include <config.h>
#include <software_outdated.h>
#include <util/system.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <functional>
#include <string>

BOOST_FIXTURE_TEST_SUITE(rpc_server_tests, TestingSetup)

class ArgsTestRPCCommand : public RPCCommandWithArgsContext {
public:
    ArgsTestRPCCommand(const std::string &nameIn)
        : RPCCommandWithArgsContext(nameIn) {}

    UniValue Execute(const UniValue &args) const override {
        BOOST_CHECK_EQUAL(args["arg1"].get_str(), "value1");
        return UniValue("testing1");
    }
};

static bool isRpcMethodNotFound(const UniValue &u) {
    return u["code"].get_int() == int(RPC_METHOD_NOT_FOUND);
}

static bool isRpcDisabled(const UniValue &u) {
    return u["code"].get_int() == int(RPC_DISABLED);
}

struct Defer {
    const std::function<void()> func;
    template <typename F>
    Defer(F && f) : func(f) {}
    ~Defer() { func(); }
};

BOOST_AUTO_TEST_CASE(rpc_server_execute_command) {
    DummyConfig config;
    RPCServer rpcServer;
    const std::string commandName = "testcommand1";
    rpcServer.RegisterCommand(
        std::make_unique<ArgsTestRPCCommand>(commandName));

    UniValue args(UniValue::VOBJ);
    args.pushKV("arg1", "value1");

    // Registered commands execute and return values correctly
    JSONRPCRequest request;
    request.strMethod = commandName;
    request.params = args;
    UniValue output = rpcServer.ExecuteCommand(config, request);
    BOOST_CHECK_EQUAL(output.get_str(), "testing1");

    // Not-registered commands throw an exception as expected
    JSONRPCRequest badCommandRequest;
    badCommandRequest.strMethod = "this-command-does-not-exist";
    BOOST_CHECK_EXCEPTION(rpcServer.ExecuteCommand(config, badCommandRequest),
                          UniValue, isRpcMethodNotFound);

    // Try do disable RPC as the software_outdated mechanism would do
    // and check that the correct RPC error occurs
    BOOST_CHECK_NO_THROW(rpcServer.ExecuteCommand(config, request));
    Defer deferred([]{software_outdated::fRPCDisabled = false;}); // RAII undo on scope-end
    // disable RPC and ensure no RPC commands will work -- they all throw UniValue now
    software_outdated::fRPCDisabled = true;
    // check that the exception is what we expect
    BOOST_CHECK_EXCEPTION(rpcServer.ExecuteCommand(config, request), UniValue, isRpcDisabled);
    software_outdated::fRPCDisabled = false; // undo the disable
    BOOST_CHECK_NO_THROW(rpcServer.ExecuteCommand(config, request)); // check again
}

class RequestContextRPCCommand : public RPCCommand {
public:
    RequestContextRPCCommand(const std::string &nameIn) : RPCCommand(nameIn) {}

    // Sanity check that Execute(JSONRPCRequest) is called correctly from
    // RPCServer
    UniValue Execute(const JSONRPCRequest &request) const override {
        BOOST_CHECK_EQUAL(request.strMethod, "testcommand2");
        BOOST_CHECK_EQUAL(request.params["arg2"].get_str(), "value2");
        return UniValue("testing2");
    }
};

BOOST_AUTO_TEST_CASE(rpc_server_execute_command_from_request_context) {
    DummyConfig config;
    RPCServer rpcServer;
    const std::string commandName = "testcommand2";
    rpcServer.RegisterCommand(
        std::make_unique<RequestContextRPCCommand>(commandName));

    UniValue args(UniValue::VOBJ);
    args.pushKV("arg2", "value2");

    // Registered commands execute and return values correctly
    JSONRPCRequest request;
    request.strMethod = commandName;
    request.params = args;
    UniValue output = rpcServer.ExecuteCommand(config, request);
    BOOST_CHECK_EQUAL(output.get_str(), "testing2");
}

BOOST_AUTO_TEST_SUITE_END()
