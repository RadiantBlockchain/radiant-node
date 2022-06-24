// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2018 The Bitcoin Core developers
// Copyright (c) 2018-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rpc/server.h>

#include <config.h>
#include <fs.h>
#include <key_io.h>
#include <random.h>
#include <rpc/util.h>
#include <shutdown.h>
#include <software_outdated.h>
#include <sync.h>
#include <ui_interface.h>
#include <util/strencodings.h>
#include <util/string.h>
#include <util/system.h>

#include <univalue.h>

#include <boost/signals2/signal.hpp>

#include <memory> // for unique_ptr
#include <set>
#include <unordered_map>

static RecursiveMutex cs_rpcWarmup;
static std::atomic<bool> g_rpc_running{false};
static bool fRPCInWarmup GUARDED_BY(cs_rpcWarmup) = true;
static std::string
    rpcWarmupStatus GUARDED_BY(cs_rpcWarmup) = "RPC server started";
/* Timer-creating functions */
static RPCTimerInterface *timerInterface = nullptr;
/* Map of name to timer. */
static Mutex g_deadline_timers_mutex;
static std::map<std::string, std::unique_ptr<RPCTimerBase>> deadlineTimers GUARDED_BY(g_deadline_timers_mutex);

struct RPCCommandExecutionInfo {
    std::string method;
    int64_t start;
};

struct RPCServerInfo {
    Mutex mutex;
    std::list<RPCCommandExecutionInfo> active_commands GUARDED_BY(mutex);
};

static RPCServerInfo g_rpc_server_info;

struct RPCCommandExecution {
    std::list<RPCCommandExecutionInfo>::iterator it;
    explicit RPCCommandExecution(const std::string &method) {
        LOCK(g_rpc_server_info.mutex);
        it = g_rpc_server_info.active_commands.insert(
            g_rpc_server_info.active_commands.cend(),
            {method, GetTimeMicros()});
    }
    ~RPCCommandExecution() {
        LOCK(g_rpc_server_info.mutex);
        g_rpc_server_info.active_commands.erase(it);
    }
};

UniValue RPCServer::ExecuteCommand(Config &config,
                                   const JSONRPCRequest &request) const {
    // Return immediately if in warmup
    // This is retained from the old RPC implementation because a lot of state
    // is set during warmup that RPC commands may depend on.  This can be
    // safely removed once global variable usage has been eliminated.
    {
        LOCK(cs_rpcWarmup);
        if (fRPCInWarmup) {
            throw JSONRPCError(RPC_IN_WARMUP, rpcWarmupStatus);
        }
    }

    const auto &commandName = request.strMethod;

    // Software expired check. If this flag is set we return an error. We do
    // allow the "stop" command (so that we may shut down the daemon).
    if (software_outdated::fRPCDisabled.load(std::memory_order_relaxed)
            && commandName != "stop") {
        throw JSONRPCError(RPC_DISABLED, software_outdated::GetRPCDisabledString());
    }

    {
        auto commandsReadView = commands.getReadView();
        auto iter = commandsReadView->find(commandName);
        if (iter != commandsReadView.end()) {
            return iter->second.get()->Execute(request);
        }
    }

    // TODO Remove the below call to tableRPC.execute() and only call it for
    // context-free RPC commands via an implementation of RPCCommand.

    // Check if context-free RPC method is valid and execute it
    return tableRPC.execute(config, request);
}

void RPCServer::RegisterCommand(std::unique_ptr<RPCCommand> command) {
    if (command != nullptr) {
        const std::string &commandName = command->GetName();
        commands.getWriteView()->insert(
            std::make_pair(commandName, std::move(command)));
    }
}

static struct CRPCSignals {
    boost::signals2::signal<void()> Started;
    boost::signals2::signal<void()> Stopped;
} g_rpcSignals;

void RPCServerSignals::OnStarted(std::function<void()> slot) {
    g_rpcSignals.Started.connect(slot);
}

void RPCServerSignals::OnStopped(std::function<void()> slot) {
    g_rpcSignals.Stopped.connect(slot);
}

void RPCTypeCheck(const UniValue &params, std::initializer_list<int> expectedTypeMasks) {
    UniValue::size_type index = 0;
    for (auto expectedTypeMask : expectedTypeMasks) {
        if (params.size() <= index) {
            break;
        }
        if (!params[index].is(expectedTypeMask)) {
            throw JSONRPCError(RPC_TYPE_ERROR,
                               strprintf("Expected type %s at index %u, got %s",
                                         uvTypeName(expectedTypeMask), index,
                                         uvTypeName(params[index].type())));
        }
        index++;
    }
}

void RPCTypeCheckArgument(const UniValue &value, int expectedTypeMask) {
    if (!value.is(expectedTypeMask)) {
        throw JSONRPCError(RPC_TYPE_ERROR,
                           strprintf("Expected type %s, got %s",
                                     uvTypeName(expectedTypeMask),
                                     uvTypeName(value.type())));
    }
}

void RPCTypeCheckObj(const UniValue::Object &o, std::initializer_list<std::pair<const char *, int>> expectedTypeMasks) {
    for (auto & [expectedKey, expectedTypeMask] : expectedTypeMasks) {
        const UniValue *value = o.locate(expectedKey);
        if (value) {
            // Key found, so check value type.
            if (!value->is(expectedTypeMask)) {
                throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Expected type %s for %s, got %s",
                                                             uvTypeName(expectedTypeMask), expectedKey,
                                                             uvTypeName(value->type())));
            }
        } else if (!(expectedTypeMask & UniValue::VNULL)) {
            // Key not found, but it is required (null not accepted).
            throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Missing %s", expectedKey));
        }
    }
}

void RPCTypeCheckObjStrict(const UniValue::Object &o, std::initializer_list<std::pair<const char *, int>> expectedTypeMasks) {
    RPCTypeCheckObj(o, expectedTypeMasks);
    for (auto & [key, value] : o) {
        for (auto & [expectedKey, expectedTypeMask] : expectedTypeMasks) {
            if (key == expectedKey) {
                goto expected;
            }
        }
        throw JSONRPCError(RPC_TYPE_ERROR, strprintf("Unexpected key %s", key));
        expected:;
    }
}

Amount AmountFromValue(const UniValue &value) {
    if (!value.isNum() && !value.isStr()) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount is not a number or string");
    }

    int64_t n;
    if (!ParseFixedPoint(value.getValStr(), 8, &n)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Invalid amount");
    }

    Amount amt = n * SATOSHI;
    if (!MoneyRange(amt)) {
        throw JSONRPCError(RPC_TYPE_ERROR, "Amount out of range");
    }

    return amt;
}

uint256 ParseHashV(const UniValue &v, const std::string& strName) {
    std::string strHex(v.get_str());
    if (64 != strHex.length()) {
        throw JSONRPCError(
            RPC_INVALID_PARAMETER,
            strprintf("%s must be of length %d (not %d, for '%s')", strName, 64,
                      strHex.length(), strHex));
    }
    // Note: IsHex("") is false
    if (!IsHex(strHex)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           strName + " must be hexadecimal string (not '" +
                               strHex + "')");
    }
    return uint256S(strHex);
}
uint256 ParseHashO(const UniValue::Object &o, const std::string& strKey) {
    return ParseHashV(o.at(strKey), strKey);
}
uint256 ParseHashO(const UniValue &o, const std::string& strKey) {
    return ParseHashV(o.at(strKey), strKey);
}
std::vector<uint8_t> ParseHexV(const UniValue &v, const std::string& strName) {
    std::string strHex;
    if (v.isStr()) {
        strHex = v.get_str();
    }
    if (!IsHex(strHex)) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           strName + " must be hexadecimal string (not '" +
                               strHex + "')");
    }

    return ParseHex(strHex);
}
std::vector<uint8_t> ParseHexO(const UniValue::Object &o, const std::string& strKey) {
    return ParseHexV(o.at(strKey), strKey);
}
std::vector<uint8_t> ParseHexO(const UniValue &o, const std::string& strKey) {
    return ParseHexV(o.at(strKey), strKey);
}

std::string CRPCTable::help(Config &config, const std::string &strCommand,
                            const JSONRPCRequest &helpreq) const {
    std::string strRet;
    std::string category;
    std::set<const ContextFreeRPCCommand *> setDone;
    std::vector<std::pair<std::string, const ContextFreeRPCCommand *>>
        vCommands;

    for (const auto &entry : mapCommands) {
        vCommands.push_back(
            std::make_pair(entry.second->category + entry.first, entry.second));
    }
    sort(vCommands.begin(), vCommands.end());

    JSONRPCRequest jreq(helpreq);
    jreq.fHelp = true;
    jreq.params = UniValue();

    for (const std::pair<std::string, const ContextFreeRPCCommand *> &command :
         vCommands) {
        const ContextFreeRPCCommand *pcmd = command.second;
        std::string strMethod = pcmd->name;
        if ((strCommand != "" || pcmd->category == "hidden") &&
            strMethod != strCommand) {
            continue;
        }

        jreq.strMethod = strMethod;
        try {
            if (setDone.insert(pcmd).second) {
                pcmd->call(config, jreq);
            }
        } catch (const std::exception &e) {
            // Help text is returned in an exception
            std::string strHelp = std::string(e.what());
            if (strCommand == "") {
                if (strHelp.find('\n') != std::string::npos) {
                    strHelp = strHelp.substr(0, strHelp.find('\n'));
                }

                if (category != pcmd->category) {
                    if (!category.empty()) {
                        strRet += "\n";
                    }
                    category = pcmd->category;
                    strRet += "== " + Capitalize(category) + " ==\n";
                }
            }
            strRet += strHelp + "\n";
        }
    }
    if (strRet == "") {
        strRet = strprintf("help: unknown command: %s\n", strCommand);
    }

    strRet = strRet.substr(0, strRet.size() - 1);
    return strRet;
}

static UniValue help(Config &config, const JSONRPCRequest &jsonRequest) {
    if (jsonRequest.fHelp || jsonRequest.params.size() > 1) {
        throw std::runtime_error(RPCHelpMan{
            "help",
            "\nList all commands, or get help for a specified command.\n",
            {
                {"command", RPCArg::Type::STR, /* opt */ true, /* default_val */ "", "The command to get help on"},
            },
            RPCResult{"\"text\"     (string) The help text\n"},
            RPCExamples{""},
        }.ToStringWithResultsAndExamples());
    }

    std::string strCommand;
    if (jsonRequest.params.size() > 0) {
        strCommand = jsonRequest.params[0].get_str();
    }

    return tableRPC.help(config, strCommand, jsonRequest);
}

static UniValue stop(const Config &config, const JSONRPCRequest &jsonRequest) {
    // Accept the deprecated and ignored 'detach' boolean argument
    // Also accept the hidden 'wait' integer argument (milliseconds)
    // For instance, 'stop 1000' makes the call wait 1 second before returning
    // to the client (intended for testing)
    if (jsonRequest.fHelp || jsonRequest.params.size() > 1) {
        throw std::runtime_error(RPCHelpMan{
            "stop",
            "\nStop Bitcoin server.",
            {},
            RPCResults{},
            RPCExamples{""},
        }.ToStringWithResultsAndExamples());
    }

    // Event loop will exit after current HTTP requests have been handled, so
    // this reply will get back to the client.
    StartShutdown();
    if (jsonRequest.params[0].isNum()) {
        MilliSleep(jsonRequest.params[0].get_int());
    }
    return "Bitcoin server stopping";
}

static UniValue uptime(const Config &config,
                       const JSONRPCRequest &jsonRequest) {
    if (jsonRequest.fHelp || jsonRequest.params.size() > 0) {
        throw std::runtime_error(RPCHelpMan{
            "uptime",
            "\nReturns the total uptime of the server.\n",
            {},
            RPCResult{"ttt        (numeric) The number of seconds that the server has been running\n"},
            RPCExamples{HelpExampleCli("uptime", "") +
                        HelpExampleRpc("uptime", "")},
        }.ToStringWithResultsAndExamples());
    }

    return GetTime() - GetStartupTime();
}

static UniValue getrpcinfo(const Config &config,
                           const JSONRPCRequest &request) {
    if (request.fHelp || request.params.size() > 0) {
        throw std::runtime_error(RPCHelpMan{
            "getrpcinfo",
            "\nReturns details of the RPC server.\n",
            {},
            RPCResults{},
            RPCExamples{""},
        }.ToStringWithResultsAndExamples());
    }

    LOCK(g_rpc_server_info.mutex);
    UniValue::Array active_commands;
    active_commands.reserve(g_rpc_server_info.active_commands.size());
    for (const RPCCommandExecutionInfo &info : g_rpc_server_info.active_commands) {
        UniValue::Object entry;
        entry.reserve(2);
        entry.emplace_back("method", info.method);
        entry.emplace_back("duration", GetTimeMicros() - info.start);
        active_commands.emplace_back(std::move(entry));
    }

    UniValue::Object result;
    result.reserve(1);
    result.emplace_back("active_commands", std::move(active_commands));

    return result;
}

// clang-format off
static const ContextFreeRPCCommand vRPCCommands[] = {
    //  category            name                      actor (function)        argNames
    //  ------------------- ------------------------  ----------------------  ----------
    /* Overall control/query calls */
    { "control",            "getrpcinfo",             getrpcinfo,             {}  },
    { "control",            "help",                   help,                   {"command"}  },
    { "control",            "stop",                   stop,                   {"wait"}  },
    { "control",            "uptime",                 uptime,                 {}  },
};
// clang-format on

CRPCTable::CRPCTable() {
    unsigned int vcidx;
    for (vcidx = 0; vcidx < (sizeof(vRPCCommands) / sizeof(vRPCCommands[0]));
         vcidx++) {
        const ContextFreeRPCCommand *pcmd;

        pcmd = &vRPCCommands[vcidx];
        mapCommands[pcmd->name] = pcmd;
    }
}

const ContextFreeRPCCommand *CRPCTable::
operator[](const std::string &name) const {
    std::map<std::string, const ContextFreeRPCCommand *>::const_iterator it =
        mapCommands.find(name);
    if (it == mapCommands.end()) {
        return nullptr;
    }

    return (*it).second;
}

bool CRPCTable::appendCommand(const std::string &name,
                              const ContextFreeRPCCommand *pcmd) {
    if (IsRPCRunning()) {
        return false;
    }

    // don't allow overwriting for now
    std::map<std::string, const ContextFreeRPCCommand *>::const_iterator it =
        mapCommands.find(name);
    if (it != mapCommands.end()) {
        return false;
    }

    mapCommands[name] = pcmd;
    return true;
}

void StartRPC() {
    LogPrint(BCLog::RPC, "Starting RPC\n");
    g_rpc_running = true;
    g_rpcSignals.Started();
}

void InterruptRPC() {
    LogPrint(BCLog::RPC, "Interrupting RPC\n");
    // Interrupt e.g. running longpolls
    g_rpc_running = false;
}

void StopRPC() {
    LogPrint(BCLog::RPC, "Stopping RPC\n");
    WITH_LOCK(g_deadline_timers_mutex, deadlineTimers.clear());
    DeleteAuthCookie();
    g_rpcSignals.Stopped();
}

bool IsRPCRunning() {
    return g_rpc_running;
}

void SetRPCWarmupStatus(const std::string &newStatus) {
    LOCK(cs_rpcWarmup);
    rpcWarmupStatus = newStatus;
}

void SetRPCWarmupFinished() {
    LOCK(cs_rpcWarmup);
    assert(fRPCInWarmup);
    fRPCInWarmup = false;
}

bool RPCIsInWarmup(std::string *outStatus) {
    LOCK(cs_rpcWarmup);
    if (outStatus) {
        *outStatus = rpcWarmupStatus;
    }
    return fRPCInWarmup;
}

bool IsDeprecatedRPCEnabled(const ArgsManager &args,
                            const std::string &method) {
    const std::vector<std::string> enabled_methods =
        args.GetArgs("-deprecatedrpc");

    return find(enabled_methods.begin(), enabled_methods.end(), method) !=
           enabled_methods.end();
}

static UniValue::Object JSONRPCExecOne(Config &config, RPCServer &rpcServer, JSONRPCRequest jreq, UniValue &&req) {
    try {
        jreq.parse(std::move(req));
        // id is copied rather than moved, so it's still there for exception handlers below
        return JSONRPCReplyObj(rpcServer.ExecuteCommand(config, jreq), UniValue(), UniValue(jreq.id));
    } catch (JSONRPCError &error) {
        return JSONRPCReplyObj(UniValue(), std::move(error).toObj(), std::move(jreq.id));
    } catch (const std::exception &e) {
        return JSONRPCReplyObj(UniValue(), JSONRPCError(RPC_PARSE_ERROR, e.what()).toObj(), std::move(jreq.id));
    }
}

std::string JSONRPCExecBatch(Config &config, RPCServer &rpcServer, const JSONRPCRequest &jreq, UniValue::Array &&vReq) {
    UniValue::Array ret;
    ret.reserve(vReq.size());
    for (UniValue& req: vReq) {
        ret.emplace_back(JSONRPCExecOne(config, rpcServer, jreq, std::move(req)));
    }

    return UniValue::stringify(ret) + '\n';
}

/**
 * Process named arguments into a vector of positional arguments, based on the
 * passed-in specification for the RPC call's arguments.
 */
static inline JSONRPCRequest
transformNamedArguments(const JSONRPCRequest &in,
                        const std::vector<std::string> &argNames) {
    JSONRPCRequest out = in;
    UniValue::Array& outParams = out.params.setArray();
    // Build a map of parameters, and remove ones that have been processed, so
    // that we can throw a focused error if there is an unknown one.
    std::unordered_map<std::string, const UniValue *> argsIn;
    for (auto &entry : in.params.get_obj()) {
        argsIn[entry.first] = &entry.second;
    }
    // Process expected parameters.
    int hole = 0;
    for (const std::string &argNamePattern : argNames) {
        std::vector<std::string> vargNames;
        Split(vargNames, argNamePattern, "|");
        auto fr = argsIn.end();
        for (const std::string &argName : vargNames) {
            fr = argsIn.find(argName);
            if (fr != argsIn.end()) {
                break;
            }
        }
        if (fr != argsIn.end()) {
            for (int i = 0; i < hole; ++i) {
                // Fill hole between specified parameters with JSON nulls, but
                // not at the end (for backwards compatibility with calls that
                // act based on number of specified parameters).
                outParams.emplace_back();
            }
            hole = 0;
            outParams.push_back(*fr->second);
            argsIn.erase(fr);
        } else {
            hole += 1;
        }
    }
    // If there are still arguments in the argsIn map, this is an error.
    if (!argsIn.empty()) {
        throw JSONRPCError(RPC_INVALID_PARAMETER,
                           "Unknown named parameter " + argsIn.begin()->first);
    }
    // Return request with named arguments transformed to positional arguments
    return out;
}

UniValue CRPCTable::execute(Config &config,
                            const JSONRPCRequest &request) const {
    // Return immediately if in warmup
    {
        LOCK(cs_rpcWarmup);
        if (fRPCInWarmup) {
            throw JSONRPCError(RPC_IN_WARMUP, rpcWarmupStatus);
        }
    }

    // Check if legacy RPC method is valid.
    // See RPCServer::ExecuteCommand for context-sensitive RPC commands.
    const ContextFreeRPCCommand *pcmd = tableRPC[request.strMethod];
    if (!pcmd) {
        throw JSONRPCError(RPC_METHOD_NOT_FOUND, "Method not found");
    }

    try {
        RPCCommandExecution execution(request.strMethod);
        // Execute, convert arguments to array if necessary
        if (request.params.isObject()) {
            return pcmd->call(config,
                              transformNamedArguments(request, pcmd->argNames));
        } else {
            return pcmd->call(config, request);
        }
    } catch (const std::exception &e) {
        throw JSONRPCError(RPC_MISC_ERROR, e.what());
    }
}

std::vector<std::string> CRPCTable::listCommands() const {
    std::vector<std::string> commandList;
    for (const auto &i : mapCommands) {
        commandList.emplace_back(i.first);
    }
    return commandList;
}

std::string HelpExampleCli(const std::string &methodname,
                           const std::string &args) {
    if (args.empty()) {
        return "> radiant-cli " + methodname + "\n";
    } else {
        return "> radiant-cli " + methodname + " " + args + "\n";
    }
}

std::string HelpExampleRpc(const std::string &methodname,
                           const std::string &args) {
    return "> curl --user myusername --data-binary '{\"jsonrpc\": \"1.0\", "
           "\"id\":\"curltest\", "
           "\"method\": \"" +
           methodname + "\", \"params\": [" + args +
           "] }' -H 'content-type: text/plain;' http://127.0.0.1:8332/\n";
}

void RPCSetTimerInterfaceIfUnset(RPCTimerInterface *iface) {
    if (!timerInterface) {
        timerInterface = iface;
    }
}

void RPCSetTimerInterface(RPCTimerInterface *iface) {
    timerInterface = iface;
}

void RPCUnsetTimerInterface(RPCTimerInterface *iface) {
    if (timerInterface == iface) {
        timerInterface = nullptr;
    }
}

void RPCRunLater(const std::string &name, std::function<void()> func,
                 int64_t nSeconds) {
    if (!timerInterface) {
        throw JSONRPCError(RPC_INTERNAL_ERROR,
                           "No timer handler registered for RPC");
    }
    LOCK(g_deadline_timers_mutex);
    deadlineTimers.erase(name);
    LogPrint(BCLog::RPC, "queue run of timer %s in %i seconds (using %s)\n",
             name, nSeconds, timerInterface->Name());
    deadlineTimers.emplace(
        name, std::unique_ptr<RPCTimerBase>(
                  timerInterface->NewTimer(func, nSeconds * 1000)));
}

int RPCSerializationFlags() {
    return 0;
}

CRPCTable tableRPC;
