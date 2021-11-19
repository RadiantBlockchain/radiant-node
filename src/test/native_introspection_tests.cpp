// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <core_io.h>
#include <policy/policy.h>
#include <primitives/blockhash.h>
#include <random.h>
#include <script/interpreter.h>
#include <script/script.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>

BOOST_FIXTURE_TEST_SUITE(native_introspection_tests, BasicTestingSetup)
using valtype = std::vector<uint8_t>;
using stacktype = std::vector<valtype>;


namespace detail {
    template <typename T, typename F, std::size_t... Is>
    constexpr
    std::array<T, sizeof...(Is)> genArr(F& f, std::index_sequence<Is...>) {
        return {{f(std::integral_constant<std::size_t, Is>{})...}};
    }
}

template <typename T, std::size_t N, typename F>
constexpr
auto genArr(F&& f) {
    return detail::genArr<T>(f, std::make_index_sequence<N>{});
}

static
void CheckErrorWithFlags(uint32_t flags, stacktype const& original_stack, CScript const& script, ScriptExecutionContextOpt const& context, ScriptError expected) {
    BaseSignatureChecker sigchecker;
    ScriptError err = ScriptError::OK;
    stacktype stack{original_stack};
    bool r = EvalScript(stack, script, flags, sigchecker, context, &err);
    BOOST_CHECK(!r);
    BOOST_CHECK(err == expected);
}

static
void CheckPassWithFlags(uint32_t flags, stacktype const& original_stack, CScript const& script, ScriptExecutionContextOpt const& context, stacktype const& expected) {
    BaseSignatureChecker sigchecker;
    ScriptError err = ScriptError::OK;
    stacktype stack{original_stack};
    bool r = EvalScript(stack, script, flags, sigchecker, context, &err);
    BOOST_CHECK(r);
    BOOST_CHECK(err == ScriptError::OK);
    BOOST_CHECK(stack == expected);
}

static
std::vector<uint8_t> MakeOversizedData(size_t targetSize = MAX_SCRIPT_ELEMENT_SIZE + 1) {
    std::vector<uint8_t> ret;
    ret.reserve(targetSize);
    // fill with random bytes (32 bytes at a time since that is the limit of GetRandBytes())
    for (size_t size = ret.size(); size < targetSize; size = ret.size()) {
        constexpr size_t chunkSize = 32;
        size_t const nBytes = std::min<size_t>(chunkSize, targetSize - size);
        ret.resize(size + nBytes);
        GetRandBytes(ret.data() + size, nBytes);
    }
    return ret;
}

static
CScript MakeOversizedScript(bool pushOnly = false) {
    // Generate an "oversized" script, that is, a script that exceeds MAX_SCRIPT_ELEMENT_SIZE, by being
    // composed of many smaller 32-byte pushes.
    size_t const chunkSize = 32;
    CScript ret;
    if (pushOnly) {
        // for scriptSigs, "push only" -- keep pushing 32-byte blobs until we exceed the total size
        while (ret.size() < MAX_SCRIPT_ELEMENT_SIZE + 1) {
            ret << MakeOversizedData(chunkSize);
        }
    } else {
        // for scriptPubKeys, keep pushing an untaken branch
        while (ret.size() < MAX_SCRIPT_ELEMENT_SIZE + 1) {
            ret << OP_0 << OP_IF << MakeOversizedData(chunkSize) << OP_ENDIF;
        }
    }
    return ret;
}

BOOST_AUTO_TEST_CASE(opcodes_basic) {
    uint32_t const flags = MANDATORY_SCRIPT_VERIFY_FLAGS | SCRIPT_NATIVE_INTROSPECTION;
    uint32_t const flags_inactive = flags & ~SCRIPT_NATIVE_INTROSPECTION;

    CCoinsView dummy;
    CCoinsViewCache coins(&dummy);

    COutPoint const ins[] = {
        {TxId(uint256S("be89ae9569526343105994a950775869a910f450d337a6c29d43a37f093b662f")), 5},
        {TxId(uint256S("08d5fc002b094fced39381b7e9fa15fb8c944164e48262a2c0b8edef9866b348")), 7},
        {TxId(uint256S("64ee0a1cf5bdb83d4882204c49fe3b01b91d5988690ec7f06bc246f4838e2f9a")), 8},
        {TxId(uint256S("b882f60dd5ff9c6145490ae7184c30b68ff81b8234bd22ce69ae93804ebc0e49")), 2},
        {TxId(uint256S("c0fdde4ecc648b38f0945989fb028fa8e82c958bd3ba44016be2f346e0172916")), 4},
        {TxId(uint256S("641c630845e84d6e9be940f062d52bbef38f78f9a21014d3c0c8248fcae89b9b")), 1},
        {TxId(uint256S("9840c9252d8c0de8b2648683443e40af619bce890d19f04b81cfbf267efeba35")), 5},
        {TxId(uint256S("120e438cd283fa46484979c90e11648cba296d8f5cff8624b3ed8950bbfbe0e0")), 3},
        {TxId(uint256S("8024808fd4e7959b342f0b1e4c1254edb1f60edfcb928463a8098c9f3c6eba86")), 2},
        {TxId(uint256S("109dafc04e629809fbf64c04abe76fe2835398848c28b44cfddb203ee91b5816")), 8},
        {TxId(uint256S("30824bf6be8c656d894f48b2ee900130d720fe969bcce4f19c8d24fa8ba83145")), 4},
        {TxId(uint256S("eeedb9492f570482e1b4460894b22f83163cf8053cd8ae81d9604c6f0cf8a9bb")), 0},
        {TxId(uint256S("81766a636f99138dee8200bfba55ff124bcdb424cde8e932ccb8e4890004f984")), 3},
        {TxId(uint256S("943aebc64feed8112af2bc065297bc84f1b28940e8ddb0ff35948963886c0e40")), 2},
        {TxId(uint256S("1b86fb3052e98e86254ebaec891442c960f803b2ce4b40f470fd9df6dca18893")), 6},
        {TxId(uint256S("e64f16f94392e8f0564be4858d8bbc300c21a8c079b0f836bec28ad94ed9f421")), 4},
        {TxId(uint256S("4fe8ec6dde591cb34196bb4c54beab863492628a8109fd38f7fce9808f004202")), 7},
        {TxId(uint256S("ed84f34d806b8900f822459f203b6e9c1a0bb963f8d81b4c5eeec2ca4761489f")), 6},
        {TxId(uint256S("f79103534dfe073d2de397673b72baf2b75d6ae21005c5685305878a4c6cbcab")), 0},
    };

    size_t const oversized_in = std::size(ins) - 1; // this input's scriptPubKey and scriptSig both exceed MAX_SCRIPT_ELEMENT_SIZE

    constexpr auto vals = genArr<Amount, std::size(ins)>([](auto inNum) {
        return (2000 + int(inNum) * 1000) * Amount::satoshi();
    });
    auto const coinScriptPubKeys = genArr<CScript, std::size(ins)>([&oversized_in](auto inNum) {
        CScript ret;
        if (inNum == oversized_in) {
            // Make a script consisting of many small pushes on an untaken branch.
            // We add OP_CODESEPARATOR to the end to test that OP_UTXOBYTECODE ignores this op-code.
            ret = MakeOversizedScript() << OP_CODESEPARATOR;
        } else {
            ret << ScriptInt::fromInt(2 + inNum).value() << OP_ADD << OP_0 << OP_GREATERTHAN;
        }
        return ret;
    });

    COutPoint const& in0 = ins[0];
    COutPoint const& in1 = ins[1];
    Amount const& val0 = vals[0];
    Amount const& val1 = vals[1];
    CScript const& coinScriptPubKey0 = coinScriptPubKeys[0];
    CScript const& coinScriptPubKey1 = coinScriptPubKeys[1];

    CMutableTransaction tx;
    tx.vin.resize(std::size(ins));

    {
        for (size_t i = 0; i < std::size(ins); ++i) {
            coins.AddCoin(ins[i], Coin(CTxOut(vals[i], coinScriptPubKeys[i]), 1, false), false);
            tx.vin[i].prevout = ins[i];
            if (i == oversized_in) {
                // large scriptsig here (lots of smaller pushes)
                tx.vin[i].scriptSig = MakeOversizedScript(true /* push only*/);
            } else {
                tx.vin[i].scriptSig = CScript() << ScriptInt::fromInt(i).value();
            }
            tx.vin[i].nSequence = 0x010203;
        }
    }

    tx.vout.resize(4);
    tx.vout[0].nValue = 1000 * Amount::satoshi();
    tx.vout[0].scriptPubKey = CScript() << OP_2;
    tx.vout[1].nValue = 1900 * Amount::satoshi();
    tx.vout[1].scriptPubKey = CScript() << OP_3;
    tx.vout[2].nValue = 2100 * Amount::satoshi();
    tx.vout[2].scriptPubKey = CScript() << OP_4;
    tx.vout[3].nValue = 3100 * Amount::satoshi();
    // We append OP_CODESEPARATOR to the scriptPubKey to check that OP_OUTPUTBYTECODE ignores this op-code.
    tx.vout[3].scriptPubKey = MakeOversizedScript() << OP_CODESEPARATOR;
    tx.nVersion = 101;
    tx.nLockTime = 10;

    size_t const oversized_out = tx.vout.size() - 1;

    auto const context = ScriptExecutionContext::createForAllInputs(tx, coins);

    std::vector<ScriptExecutionContext> limited_context;
    limited_context.reserve(context.size());
    for (auto& ctx : context) {
        limited_context.emplace_back(ctx.inputIndex(), ctx.coinScriptPubKey(), ctx.coinAmount(), tx,
                                     ctx.coin().GetHeight(), ctx.coin().IsCoinBase());
    }

    BOOST_CHECK(context.size() == tx.vin.size());

    // OP_INPUTINDEX (nullary)
    {
        valtype const expected0 (CScriptNum::fromInt(0).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_INPUTINDEX, context[0], {expected0});

        valtype const expected1 (CScriptNum::fromInt(1).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_INPUTINDEX, context[1], {expected1});

        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_INPUTINDEX, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_INPUTINDEX, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_ACTIVEBYTECODE (nullary)
    {
        auto const bytecode0 = CScript() << OP_ACTIVEBYTECODE << OP_9;
        auto const bytecode1 = CScript() << OP_ACTIVEBYTECODE << OP_10;

        auto const bytecode2 = CScript() << OP_10
                                         << OP_11
                                         << ScriptInt::fromInt(7654321).value()
                                         << OP_CODESEPARATOR
                                         << ScriptInt::fromInt(123123).value() << OP_DROP
                                         << OP_ACTIVEBYTECODE
                                         << OP_CODESEPARATOR
                                         << OP_1;
        auto const bytecode2b = CScript() << ScriptInt::fromInt(123123).value() << OP_DROP
                                          << OP_ACTIVEBYTECODE
                                          << OP_CODESEPARATOR
                                          << OP_1;

        auto const bytecode3 = MakeOversizedScript() << OP_CODESEPARATOR << OP_ACTIVEBYTECODE << OP_1;
        auto const bytecode3b = CScript() << OP_ACTIVEBYTECODE << OP_1;

        auto const bytecode4 = MakeOversizedScript() << OP_ACTIVEBYTECODE << OP_1;

        valtype const expected0 (bytecode0.begin(), bytecode0.end());
        CheckPassWithFlags(flags, {}, bytecode0, context[0], {expected0, CScriptNum::fromIntUnchecked(9).getvch()});

        valtype const expected1 (bytecode1.begin(), bytecode1.end());
        CheckPassWithFlags(flags, {}, bytecode1, context[0], {expected1, CScriptNum::fromIntUnchecked(10).getvch()});

        // check that OP_CODESEPARATOR is respected properly
        valtype const expected2b (bytecode2b.begin(), bytecode2b.end());
        CheckPassWithFlags(flags, {}, bytecode2, context[0], {CScriptNum::fromIntUnchecked(10).getvch(),
                                                              CScriptNum::fromIntUnchecked(11).getvch(),
                                                              CScriptNum::fromIntUnchecked(7654321).getvch(),
                                                              expected2b,
                                                              CScriptNum::fromIntUnchecked(1).getvch()});

        // ScriptError::PUSH_SIZE should *not* be triggered if using OP_CODESEPARATOR and result would be under
        // MAX_SCRIPT_ELEMENT_SIZE even if entire script is over MAX_SCRIPT_ELEMENT_SIZE.
        valtype const expected3b (bytecode3b.begin(), bytecode3b.end());
        CheckPassWithFlags(flags, {}, bytecode3, context[oversized_in], {expected3b, CScriptNum::fromIntUnchecked(1).getvch()});

        // failure (MAX_SCRIPT_ELEMENT_SIZE exceeded)
        CheckErrorWithFlags(flags, {}, bytecode4, context[oversized_in], ScriptError::PUSH_SIZE);
        // failure (no context)
        CheckErrorWithFlags(flags, {}, bytecode1, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, bytecode1, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_TXVERSION (nullary)
    {
        valtype const expected (CScriptNum::fromInt(tx.nVersion).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_TXVERSION, context[0], {expected});
        CheckPassWithFlags(flags, {}, CScript() << OP_TXVERSION, context[1], {expected});

        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_TXVERSION, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_TXVERSION, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_TXINPUTCOUNT (nullary)
    {
        valtype const expected (CScriptNum::fromInt(tx.vin.size()).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_TXINPUTCOUNT, context[0], {expected});
        CheckPassWithFlags(flags, {}, CScript() << OP_TXINPUTCOUNT, context[1], {expected});

        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_TXINPUTCOUNT, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_TXINPUTCOUNT, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_TXOUTPUTCOUNT (nullary)
    {
        valtype const expected (CScriptNum::fromInt(tx.vout.size()).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_TXOUTPUTCOUNT, context[0], {expected});
        CheckPassWithFlags(flags, {}, CScript() << OP_TXOUTPUTCOUNT, context[1], {expected});

        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_TXOUTPUTCOUNT, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_TXOUTPUTCOUNT, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_TXLOCKTIME (nullary)
    {
        valtype const expected (CScriptNum::fromInt(tx.nLockTime).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_TXLOCKTIME, context[0], {expected});
        CheckPassWithFlags(flags, {}, CScript() << OP_TXLOCKTIME, context[1], {expected});

        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_TXLOCKTIME, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_TXLOCKTIME, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_UTXOVALUE (unary)
    {
        valtype const expected0 (CScriptNum::fromInt(val0 / SATOSHI).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOVALUE, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOVALUE, context[1], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOVALUE, limited_context[0], {expected0});

        valtype const expected1 (CScriptNum::fromInt(val1 / SATOSHI).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOVALUE, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOVALUE, context[1], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOVALUE, limited_context[1], {expected1});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_UTXOVALUE, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vin.size()).value() << OP_UTXOVALUE,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_UTXOVALUE,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOVALUE, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (limited context but querying sibling input)
        CheckErrorWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOVALUE, limited_context[0],
                            ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOVALUE, limited_context[1],
                            ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_UTXOVALUE, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_UTXOBYTECODE (unary)
    {
        valtype const expected0 (coinScriptPubKey0.begin(), coinScriptPubKey0.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOBYTECODE, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOBYTECODE, context[1], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOBYTECODE, limited_context[0], {expected0});

        valtype const expected1 (coinScriptPubKey1.begin(), coinScriptPubKey1.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOBYTECODE, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOBYTECODE, context[1], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOBYTECODE, limited_context[1], {expected1});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_UTXOBYTECODE, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vin.size()).value() << OP_UTXOBYTECODE,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_UTXOBYTECODE,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        // failure (MAX_SCRIPT_ELEMENT_SIZE exceeded)
        for (auto const& ctx : context) {
            CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(oversized_in).value() << OP_UTXOBYTECODE,
                                ctx, ScriptError::PUSH_SIZE);
        }
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOBYTECODE, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (limited context but querying sibling input)
        CheckErrorWithFlags(flags, {}, CScript() << OP_1 << OP_UTXOBYTECODE, limited_context[0],
                            ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_UTXOBYTECODE, limited_context[1],
                            ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_UTXOBYTECODE, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_OUTPOINTTXHASH (unary)
    {
        valtype const expected0 (in0.GetTxId().begin(), in0.GetTxId().end());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPOINTTXHASH, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPOINTTXHASH, context[1], {expected0});

        valtype const expected1 (in1.GetTxId().begin(), in1.GetTxId().end());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPOINTTXHASH, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPOINTTXHASH, context[1], {expected1});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_OUTPOINTTXHASH, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vin.size()).value() << OP_OUTPOINTTXHASH,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_OUTPOINTTXHASH,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPOINTTXHASH, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_OUTPOINTTXHASH, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_OUTPOINTINDEX (unary)
    {
        valtype const expected0 (CScriptNum::fromInt(in0.GetN()).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPOINTINDEX, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPOINTINDEX, context[1], {expected0});

        valtype const expected1 (CScriptNum::fromInt(in1.GetN()).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPOINTINDEX, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPOINTINDEX, context[1], {expected1});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_OUTPOINTINDEX, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vin.size()).value() << OP_OUTPOINTINDEX,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_OUTPOINTINDEX,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPOINTINDEX, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_OUTPOINTINDEX, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_INPUTBYTECODE (unary)
    {
        valtype const expected0 (tx.vin[0].scriptSig.begin(), tx.vin[0].scriptSig.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_INPUTBYTECODE, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_INPUTBYTECODE, context[1], {expected0});

        valtype const expected1 (tx.vin[1].scriptSig.begin(), tx.vin[1].scriptSig.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_INPUTBYTECODE, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_INPUTBYTECODE, context[1], {expected1});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_INPUTBYTECODE, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vin.size()).value() << OP_INPUTBYTECODE,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_INPUTBYTECODE,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        // failure (MAX_SCRIPT_ELEMENT_SIZE exceeded)
        for (auto const& ctx : context) {
            CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(oversized_in).value() << OP_INPUTBYTECODE,
                                ctx, ScriptError::PUSH_SIZE);
        }
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_INPUTBYTECODE, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_INPUTBYTECODE, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_INPUTSEQUENCENUMBER (unary)
    {
        valtype const expected0 (CScriptNum::fromInt(tx.vin[0].nSequence).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_INPUTSEQUENCENUMBER, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_INPUTSEQUENCENUMBER, context[1], {expected0});

        valtype const expected1 (CScriptNum::fromInt(tx.vin[1].nSequence).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_INPUTSEQUENCENUMBER, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_INPUTSEQUENCENUMBER, context[1], {expected1});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_INPUTSEQUENCENUMBER, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vin.size()).value() << OP_INPUTSEQUENCENUMBER,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_INPUTSEQUENCENUMBER,
                            context[1], ScriptError::INVALID_TX_INPUT_INDEX);
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_INPUTSEQUENCENUMBER, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_INPUTSEQUENCENUMBER, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_OUTPUTVALUE (unary)
    {
        valtype const expected0 (CScriptNum::fromInt(tx.vout[0].nValue / SATOSHI).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPUTVALUE, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPUTVALUE, context[1], {expected0});

        valtype const expected1 (CScriptNum::fromInt(tx.vout[1].nValue / SATOSHI).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPUTVALUE, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPUTVALUE, context[1], {expected1});

        valtype const expected2 (CScriptNum::fromInt(tx.vout[2].nValue / SATOSHI).value().getvch());
        CheckPassWithFlags(flags, {}, CScript() << OP_2 << OP_OUTPUTVALUE, context[0], {expected2});
        CheckPassWithFlags(flags, {}, CScript() << OP_2 << OP_OUTPUTVALUE, context[1], {expected2});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_OUTPUTVALUE, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vout.size()).value() << OP_OUTPUTVALUE,
                            context[1], ScriptError::INVALID_TX_OUTPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_OUTPUTVALUE,
                            context[1], ScriptError::INVALID_TX_OUTPUT_INDEX);
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPUTVALUE, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_OUTPUTVALUE, context[0], ScriptError::BAD_OPCODE);
    }

    // OP_OUTPUTBYTECODE (unary)
    {
        valtype const expected0 (tx.vout[0].scriptPubKey.begin(), tx.vout[0].scriptPubKey.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPUTBYTECODE, context[0], {expected0});
        CheckPassWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPUTBYTECODE, context[1], {expected0});

        valtype const expected1 (tx.vout[1].scriptPubKey.begin(), tx.vout[1].scriptPubKey.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPUTBYTECODE, context[0], {expected1});
        CheckPassWithFlags(flags, {}, CScript() << OP_1 << OP_OUTPUTBYTECODE, context[1], {expected1});

        valtype const expected2 (tx.vout[2].scriptPubKey.begin(), tx.vout[2].scriptPubKey.end());
        CheckPassWithFlags(flags, {}, CScript() << OP_2 << OP_OUTPUTBYTECODE, context[0], {expected2});
        CheckPassWithFlags(flags, {}, CScript() << OP_2 << OP_OUTPUTBYTECODE, context[1], {expected2});

        // failure (missing arg)
        CheckErrorWithFlags(flags, {}, CScript() << OP_OUTPUTBYTECODE, context[0], ScriptError::INVALID_STACK_OPERATION);
        // failure (out of range)
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(tx.vout.size()).value() << OP_OUTPUTBYTECODE,
                            context[1], ScriptError::INVALID_TX_OUTPUT_INDEX);
        CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(-1).value() << OP_OUTPUTBYTECODE,
                            context[1], ScriptError::INVALID_TX_OUTPUT_INDEX);
        // failure (MAX_SCRIPT_ELEMENT_SIZE exceeded)
        for (auto const& ctx : context) {
            CheckErrorWithFlags(flags, {}, CScript() << ScriptInt::fromInt(oversized_out).value() << OP_OUTPUTBYTECODE,
                                ctx, ScriptError::PUSH_SIZE);
        }
        // failure (no context)
        CheckErrorWithFlags(flags, {}, CScript() << OP_0 << OP_OUTPUTBYTECODE, {}, ScriptError::CONTEXT_NOT_PRESENT);
        // failure (not activated)
        CheckErrorWithFlags(flags_inactive, {}, CScript() << OP_0 << OP_OUTPUTBYTECODE, context[0], ScriptError::BAD_OPCODE);
    }
}

BOOST_AUTO_TEST_SUITE_END()
