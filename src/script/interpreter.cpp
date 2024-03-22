// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/interpreter.h>

#include <crypto/ripemd160.h>
#include <crypto/sha1.h>
#include <crypto/sha256.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/bitfield.h>
#include <script/script.h>
#include <script/script_flags.h>
#include <script/sigencoding.h>
#include <uint256.h>
#include <util/bitmanip.h>
#include <iostream>
#include <util/strencodings.h>

inline uint8_t make_rshift_mask(size_t n) {
    static uint8_t mask[] = {0xFF, 0xFE, 0xFC, 0xF8, 0xF0, 0xE0, 0xC0, 0x80}; 
    return mask[n]; 
} 

inline uint8_t make_lshift_mask(size_t n) {
    static uint8_t mask[] = {0xFF, 0x7F, 0x3F, 0x1F, 0x0F, 0x07, 0x03, 0x01}; 
    return mask[n]; 
} 

// shift x right by n bits, implements OP_RSHIFT
static valtype RShift(const valtype &x, int n) {
    int bit_shift = n % 8; 
    int byte_shift = n / 8; 

    uint8_t mask = make_rshift_mask(bit_shift); 
    uint8_t overflow_mask = ~mask; 

    valtype result(x.size(), 0x00); 
    for (int i = 0; i < (int)x.size(); i++) {
        int k = i + byte_shift;
        if (k < (int)x.size()) {
            uint8_t val = (x[i] & mask); 
            val >>= bit_shift;
            result[k] |= val; 
        } 

        if (k + 1 < (int)x.size()) {
            uint8_t carryval = (x[i] & overflow_mask); 
            carryval <<= 8 - bit_shift; 
            result[k + 1] |= carryval;
        } 
    } 
    return result; 
} 

// shift x left by n bits, implements OP_LSHIFT
static valtype LShift(const valtype &x, int n) {
    int bit_shift = n % 8; 
    int byte_shift = n / 8; 

    uint8_t mask = make_lshift_mask(bit_shift); 
    uint8_t overflow_mask = ~mask; 

    valtype result(x.size(), 0x00); 
    for (int i = x.size() -1; i >= 0; i--) {
        int k = i - byte_shift;
        if (k >= 0)  {
            uint8_t val = (x[i] & mask); 
            val <<= bit_shift;
            result[k] |= val; 
        } 

        if (k - 1 >= 0) {
            uint8_t carryval = (x[i] & overflow_mask); 
            carryval >>= 8 - bit_shift;
            result[k - 1] |= carryval;
        } 
    } 
    return result; 
} 

bool CastToBool(const valtype &vch) {
    for (size_t i = 0; i < vch.size(); i++) {
        if (vch[i] != 0) {
            // Can be negative zero
            if (i == vch.size() - 1 && vch[i] == 0x80) {
                return false;
            }
            return true;
        }
    }
    return false;
}

/**
 * Script is a stack machine (like Forth) that evaluates a predicate
 * returning a bool indicating valid or not.  There are no loops.
 */
#define stacktop(i) (stack.at(stack.size() + (i)))
#define altstacktop(i) (altstack.at(altstack.size() + (i)))
static inline void popstack(std::vector<valtype> &stack) {
    if (stack.empty()) {
        throw std::runtime_error("popstack(): stack empty");
    }
    stack.pop_back();
}

int FindAndDelete(CScript &script, const CScript &b) {
    int nFound = 0;
    if (b.empty()) {
        return nFound;
    }

    CScript result;
    CScript::const_iterator pc = script.begin(), pc2 = script.begin(),
                            end = script.end();
    opcodetype opcode;
    do {
        result.insert(result.end(), pc2, pc);
        while (static_cast<size_t>(end - pc) >= b.size() &&
               std::equal(b.begin(), b.end(), pc)) {
            pc = pc + b.size();
            ++nFound;
        }
        pc2 = pc;
    } while (script.GetOp(pc, opcode));

    if (nFound > 0) {
        result.insert(result.end(), pc2, end);
        script = std::move(result);
    }

    return nFound;
}

static bool IsOpcodeDisabled(opcodetype opcode, uint32_t flags) {
    switch (opcode) {
        case OP_2MUL:
        case OP_2DIV:
        case OP_LSHIFT:
        case OP_RSHIFT:
            // Disabled opcodes.
            return true;
        case OP_REFHASHDATASUMMARY_OUTPUT:
        case OP_REFHASHVALUESUM_OUTPUTS:
        case OP_PUSHINPUTREFSINGLETON: 
        case OP_REFTYPE_UTXO:
        case OP_REFTYPE_OUTPUT:
        case OP_STATESEPARATOR:
        case OP_STATESEPARATORINDEX_UTXO:
        case OP_STATESEPARATORINDEX_OUTPUT:
        case OP_REFVALUESUM_UTXOS:
        case OP_REFVALUESUM_OUTPUTS:
        case OP_REFOUTPUTCOUNT_UTXOS:
        case OP_REFOUTPUTCOUNT_OUTPUTS:
        case OP_REFOUTPUTCOUNTZEROVALUED_UTXOS:
        case OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS:
        case OP_REFDATASUMMARY_UTXO:
        case OP_REFDATASUMMARY_OUTPUT:
        case OP_CODESCRIPTHASHVALUESUM_UTXOS:
        case OP_CODESCRIPTHASHVALUESUM_OUTPUTS:
        case OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS:
        case OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS:
        case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS:
        case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS:
        case OP_CODESCRIPTBYTECODE_UTXO:
        case OP_CODESCRIPTBYTECODE_OUTPUT:
        case OP_STATESCRIPTBYTECODE_UTXO:
        case OP_STATESCRIPTBYTECODE_OUTPUT:
            return (flags & SCRIPT_ENHANCED_REFERENCES) == 0;
        case OP_PUSH_TX_STATE:
            return (flags & SCRIPT_PUSH_TX_STATE) == 0;
        case OP_INVERT:
        case OP_MUL:
        default:
            break;
    }
    return false;
}

namespace {
/**
 * A data type to abstract out the condition stack during script execution.
 *
 * Conceptually it acts like a vector of booleans, one for each level of nested
 * IF/THEN/ELSE, indicating whether we're in the active or inactive branch of
 * each.
 *
 * The elements on the stack cannot be observed individually; we only need to
 * expose whether the stack is empty and whether or not any false values are
 * present at all. To implement OP_ELSE, a toggle_top modifier is added, which
 * flips the last value without returning it.
 *
 * This uses an optimized implementation that does not materialize the
 * actual stack. Instead, it just stores the size of the would-be stack,
 * and the position of the first false value in it.
 */
class ConditionStack {
private:
    //! A constant for m_first_false_pos to indicate there are no falses.
    static constexpr uint32_t NO_FALSE = std::numeric_limits<uint32_t>::max();

    //! The size of the implied stack.
    uint32_t m_stack_size = 0;
    //! The position of the first false value on the implied stack, or NO_FALSE
    //! if all true.
    uint32_t m_first_false_pos = NO_FALSE;

public:
    [[nodiscard]] constexpr bool empty() const noexcept { return m_stack_size == 0; }
    [[nodiscard]] constexpr bool all_true() const noexcept { return m_first_false_pos == NO_FALSE; }
    constexpr void push_back(bool f) noexcept {
        if (m_first_false_pos == NO_FALSE && !f) {
            // The stack consists of all true values, and a false is added.
            // The first false value will appear at the current size.
            m_first_false_pos = m_stack_size;
        }
        ++m_stack_size;
    }
    constexpr void pop_back() noexcept {
        --m_stack_size;
        if (m_first_false_pos == m_stack_size) {
            // When popping off the first false value, everything becomes true.
            m_first_false_pos = NO_FALSE;
        }
    }
    constexpr void toggle_top() noexcept {
        if (m_first_false_pos == NO_FALSE) {
            // The current stack is all true values; the first false will be the
            // top.
            m_first_false_pos = m_stack_size - 1;
        } else if (m_first_false_pos == m_stack_size - 1) {
            // The top is the first false value; toggling it will make
            // everything true.
            m_first_false_pos = NO_FALSE;
        } else {
            // There is a false value, but not on top. No action is needed as
            // toggling anything but the first false value is unobservable.
        }
    }
};
} // namespace

bool EvalScript(std::vector<valtype> &stack, const CScript &script,
                uint32_t flags, const BaseSignatureChecker &checker,
                ScriptExecutionMetrics &metrics, ScriptExecutionContextOpt const& context, ScriptError *serror) {
    static auto const bnZero = CScriptNum::fromIntUnchecked(0);
    static const valtype vchFalse(0);
    static const valtype vchTrue(1, 1);

    CScript::const_iterator pc = script.begin();
    CScript::const_iterator pend = script.end();
    CScript::const_iterator pbegincodehash = script.begin();
    opcodetype opcode;
    valtype vchPushValue;
    ConditionStack vfExec;
    std::vector<valtype> altstack;
    set_error(serror, ScriptError::UNKNOWN);
    if (script.size() > MAX_SCRIPT_SIZE) {
        return set_error(serror, ScriptError::SCRIPT_SIZE);
    }
    int nOpCount = 0;
    bool const fRequireMinimal = (flags & SCRIPT_VERIFY_MINIMALDATA) != 0;
    bool const nativeIntrospection = (flags & SCRIPT_NATIVE_INTROSPECTION) != 0;
    bool const enhancedReferences = (flags & SCRIPT_ENHANCED_REFERENCES) != 0;
    bool const pushTxState = (flags & SCRIPT_PUSH_TX_STATE) != 0;
    bool const integers64Bit = (flags & SCRIPT_64_BIT_INTEGERS) != 0;

    size_t const maxIntegerSize = integers64Bit ?
        CScriptNum::MAXIMUM_ELEMENT_SIZE_64_BIT :
        CScriptNum::MAXIMUM_ELEMENT_SIZE_32_BIT;

    ScriptError const invalidNumberRangeError = integers64Bit ?
        ScriptError::INVALID_NUMBER_RANGE_64_BIT :
        ScriptError::INVALID_NUMBER_RANGE;

    std::set<uint288> foundPushRefs;
    std::set<uint288> disallowedRefs;

    try {
        while (pc < pend) {
            bool fExec = vfExec.all_true();
            //
            // Read instruction
            //
            if (!script.GetOp(pc, opcode, vchPushValue)) {
                return set_error(serror, ScriptError::BAD_OPCODE);
            }
            if (vchPushValue.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                return set_error(serror, ScriptError::PUSH_SIZE);
            }

            // Note how OP_RESERVED does not count towards the opcode limit.
            if (opcode > OP_16 && ++nOpCount > MAX_OPS_PER_SCRIPT) {
                return set_error(serror, ScriptError::OP_COUNT);
            }

            // Some opcodes are disabled.
            if (IsOpcodeDisabled(opcode, flags)) {
                return set_error(serror, ScriptError::DISABLED_OPCODE);
            }

            if (fExec && 0 <= opcode && opcode <= OP_PUSHDATA4) {
                if (fRequireMinimal &&
                    !CheckMinimalPush(vchPushValue, opcode)) {
                    return set_error(serror, ScriptError::MINIMALDATA);
                }
                stack.push_back(vchPushValue);
            } else if (fExec || (OP_IF <= opcode && opcode <= OP_ENDIF)) {
                switch (opcode) {
                    //
                    // Push value
                    //
                    case OP_1NEGATE:
                    case OP_1:
                    case OP_2:
                    case OP_3:
                    case OP_4:
                    case OP_5:
                    case OP_6:
                    case OP_7:
                    case OP_8:
                    case OP_9:
                    case OP_10:
                    case OP_11:
                    case OP_12:
                    case OP_13:
                    case OP_14:
                    case OP_15:
                    case OP_16: {
                        // ( -- value)
                        auto const bn = CScriptNum::fromIntUnchecked(int(opcode) - int(OP_1 - 1));
                        stack.push_back(bn.getvch());
                        // The result of these opcodes should always be the
                        // minimal way to push the data they push, so no need
                        // for a CheckMinimalPush here.
                    } break;

                    //
                    // Control
                    //
                    case OP_NOP:
                        break;

                    case OP_CHECKLOCKTIMEVERIFY: {
                        if (!(flags & SCRIPT_VERIFY_CHECKLOCKTIMEVERIFY)) {
                            break;
                        }

                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        // Note that elsewhere numeric opcodes are limited to
                        // operands in the range -2**31+1 to 2**31-1, however it
                        // is legal for opcodes to produce results exceeding
                        // that range. This limitation is implemented by
                        // CScriptNum's default 4-byte limit.
                        //
                        // If we kept to that limit we'd have a year 2038
                        // problem, even though the nLockTime field in
                        // transactions themselves is uint32 which only becomes
                        // meaningless after the year 2106.
                        //
                        // Thus as a special case we tell CScriptNum to accept
                        // up to 5-byte bignums, which are good until 2**39-1,
                        // well beyond the 2**32-1 limit of the nLockTime field
                        // itself.
                        const CScriptNum nLockTime(stacktop(-1), fRequireMinimal, 5);

                        // In the rare event that the argument may be < 0 due to
                        // some arithmetic being done first, you can always use
                        // 0 MAX CHECKLOCKTIMEVERIFY.
                        if (nLockTime < 0) {
                            return set_error(serror, ScriptError::NEGATIVE_LOCKTIME);
                        }

                        // Actually compare the specified lock time with the
                        // transaction.
                        if (!checker.CheckLockTime(nLockTime)) {
                            return set_error(serror,
                                             ScriptError::UNSATISFIED_LOCKTIME);
                        }

                        break;
                    }

                    case OP_CHECKSEQUENCEVERIFY: {
                        if (!(flags & SCRIPT_VERIFY_CHECKSEQUENCEVERIFY)) {
                            break;
                        }

                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        // nSequence, like nLockTime, is a 32-bit unsigned
                        // integer field. See the comment in CHECKLOCKTIMEVERIFY
                        // regarding 5-byte numeric operands.
                        const CScriptNum nSequence(stacktop(-1), fRequireMinimal, 5);

                        // In the rare event that the argument may be < 0 due to
                        // some arithmetic being done first, you can always use
                        // 0 MAX CHECKSEQUENCEVERIFY.
                        if (nSequence < 0) {
                            return set_error(serror, ScriptError::NEGATIVE_LOCKTIME);
                        }

                        // To provide for future soft-fork extensibility, if the
                        // operand has the disabled lock-time flag set,
                        // CHECKSEQUENCEVERIFY behaves as a NOP.
                        auto res = nSequence.safeBitwiseAnd(CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG);
                        if ( ! res) {
                            // Defensive programming: It is impossible for the following error to be
                            // returned unless the current possible values of the operands change.
                            return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                        }
                        if (*res != 0) {
                            break;
                        }

                        // Compare the specified sequence number with the input.
                        if (!checker.CheckSequence(nSequence)) {
                            return set_error(serror, ScriptError::UNSATISFIED_LOCKTIME);
                        }
                        break;
                    }

                    case OP_NOP1:
                    case OP_NOP4:
                    case OP_NOP5:
                    case OP_NOP6:
                    case OP_NOP7:
                    case OP_NOP8:
                    case OP_NOP9:
                    case OP_NOP10: {
                        if (flags & SCRIPT_VERIFY_DISCOURAGE_UPGRADABLE_NOPS) {
                            return set_error(
                                serror,
                                ScriptError::DISCOURAGE_UPGRADABLE_NOPS);
                        }
                    } break;

                    case OP_IF:
                    case OP_NOTIF: {
                        // <expression> if [statements] [else [statements]]
                        // endif
                        bool fValue = false;
                        if (fExec) {
                            if (stack.size() < 1) {
                                return set_error(
                                    serror,
                                    ScriptError::UNBALANCED_CONDITIONAL);
                            }
                            valtype &vch = stacktop(-1);
                            if (flags & SCRIPT_VERIFY_MINIMALIF) {
                                if (vch.size() > 1) {
                                    return set_error(serror,
                                                     ScriptError::MINIMALIF);
                                }
                                if (vch.size() == 1 && vch[0] != 1) {
                                    return set_error(serror,
                                                     ScriptError::MINIMALIF);
                                }
                            }
                            fValue = CastToBool(vch);
                            if (opcode == OP_NOTIF) {
                                fValue = !fValue;
                            }
                            popstack(stack);
                        }
                        vfExec.push_back(fValue);
                    } break;

                    case OP_ELSE: {
                        if (vfExec.empty()) {
                            return set_error(
                                serror, ScriptError::UNBALANCED_CONDITIONAL);
                        }
                        vfExec.toggle_top();
                    } break;

                    case OP_ENDIF: {
                        if (vfExec.empty()) {
                            return set_error(
                                serror, ScriptError::UNBALANCED_CONDITIONAL);
                        }
                        vfExec.pop_back();
                    } break;

                    case OP_VERIFY: {
                        // (true -- ) or
                        // (false -- false) and return
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        bool fValue = CastToBool(stacktop(-1));
                        if (fValue) {
                            popstack(stack);
                        } else {
                            return set_error(serror, ScriptError::VERIFY);
                        }
                    } break;

                    case OP_RETURN: {
                        if (stack.empty()) {
                            // Terminate the execution as successful. The remaining of the script does not affect the validity (even in
                            // presence of unbalanced IFs, invalid opcodes etc)
                            return set_success(serror);
                        } else {
                            return set_error(serror, ScriptError::OP_RETURN);
                        }
                    } break;

                    //
                    // Stack ops
                    //
                    case OP_TOALTSTACK: {
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        altstack.push_back(stacktop(-1));
                        popstack(stack);
                    } break;

                    case OP_FROMALTSTACK: {
                        if (altstack.size() < 1) {
                            return set_error(
                                serror,
                                ScriptError::INVALID_ALTSTACK_OPERATION);
                        }
                        stack.push_back(altstacktop(-1));
                        popstack(altstack);
                    } break;

                    case OP_2DROP: {
                        // (x1 x2 -- )
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        popstack(stack);
                        popstack(stack);
                    } break;

                    case OP_2DUP: {
                        // (x1 x2 -- x1 x2 x1 x2)
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-2);
                        valtype vch2 = stacktop(-1);
                        stack.push_back(vch1);
                        stack.push_back(vch2);
                    } break;

                    case OP_3DUP: {
                        // (x1 x2 x3 -- x1 x2 x3 x1 x2 x3)
                        if (stack.size() < 3) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-3);
                        valtype vch2 = stacktop(-2);
                        valtype vch3 = stacktop(-1);
                        stack.push_back(vch1);
                        stack.push_back(vch2);
                        stack.push_back(vch3);
                    } break;

                    case OP_2OVER: {
                        // (x1 x2 x3 x4 -- x1 x2 x3 x4 x1 x2)
                        if (stack.size() < 4) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-4);
                        valtype vch2 = stacktop(-3);
                        stack.push_back(vch1);
                        stack.push_back(vch2);
                    } break;

                    case OP_2ROT: {
                        // (x1 x2 x3 x4 x5 x6 -- x3 x4 x5 x6 x1 x2)
                        if (stack.size() < 6) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch1 = stacktop(-6);
                        valtype vch2 = stacktop(-5);
                        stack.erase(stack.end() - 6, stack.end() - 4);
                        stack.push_back(vch1);
                        stack.push_back(vch2);
                    } break;

                    case OP_2SWAP: {
                        // (x1 x2 x3 x4 -- x3 x4 x1 x2)
                        if (stack.size() < 4) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        swap(stacktop(-4), stacktop(-2));
                        swap(stacktop(-3), stacktop(-1));
                    } break;

                    case OP_IFDUP: {
                        // (x - 0 | x x)
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-1);
                        if (CastToBool(vch)) {
                            stack.push_back(vch);
                        }
                    } break;

                    case OP_DEPTH: {
                        // -- stacksize
                        auto const bn = CScriptNum::fromIntUnchecked(stack.size());
                        stack.push_back(bn.getvch());
                    } break;

                    case OP_DROP: {
                        // (x -- )
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        popstack(stack);
                    } break;

                    case OP_DUP: {
                        // (x -- x x)
                        if (stack.size() < 1) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-1);
                        stack.push_back(vch);
                    } break;

                    case OP_NIP: {
                        // (x1 x2 -- x2)
                        if (stack.size() < 2) {
                            return set_error(
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        stack.erase(stack.end() - 2);
                    } break;

                    case OP_OVER: {
                        // (x1 x2 -- x1 x2 x1)
                        if (stack.size() < 2) {
                            return set_error(
                             
                                serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-2);
                        stack.push_back(vch);
                    } break;

                    case OP_PICK:
                    case OP_ROLL: {
                        // (xn ... x2 x1 x0 n - xn ... x2 x1 x0 xn)
                        // (xn ... x2 x1 x0 n - ... x2 x1 x0 xn)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        int64_t const n = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                        popstack(stack);
                        if (n < 0 || uint64_t(n) >= stack.size()) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype const vch = stacktop(-n - 1);
                        if (opcode == OP_ROLL) {
                            stack.erase(stack.end() - n - 1);
                        }
                        stack.push_back(vch);
                    } break;

                    case OP_ROT: {
                        // (x1 x2 x3 -- x2 x3 x1)
                        //  x2 x1 x3  after first swap
                        //  x2 x3 x1  after second swap
                        if (stack.size() < 3) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        swap(stacktop(-3), stacktop(-2));
                        swap(stacktop(-2), stacktop(-1));
                    } break;

                    case OP_SWAP: {
                        // (x1 x2 -- x2 x1)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        swap(stacktop(-2), stacktop(-1));
                    } break;

                    case OP_TUCK: {
                        // (x1 x2 -- x2 x1 x2)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype vch = stacktop(-1);
                        stack.insert(stack.end() - 2, vch);
                    } break;

                    case OP_SIZE: {
                        // (in -- in size)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        auto const bn = CScriptNum::fromIntUnchecked(stacktop(-1).size());
                        stack.push_back(bn.getvch());
                    } break;

                    //
                    // Bitwise logic
                    //
                    case OP_AND:
                    case OP_OR:
                    case OP_XOR: {
                        // (x1 x2 - out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype &vch1 = stacktop(-2);
                        valtype &vch2 = stacktop(-1);

                        // Inputs must be the same size
                        if (vch1.size() != vch2.size()) {
                            return set_error(serror, ScriptError::INVALID_OPERAND_SIZE);
                        }

                        // To avoid allocating, we modify vch1 in place.
                        switch (opcode) {
                            case OP_AND:
                                for (size_t i = 0; i < vch1.size(); ++i) {
                                    vch1[i] &= vch2[i];
                                }
                                break;
                            case OP_OR:
                                for (size_t i = 0; i < vch1.size(); ++i) {
                                    vch1[i] |= vch2[i];
                                }
                                break;
                            case OP_XOR:
                                for (size_t i = 0; i < vch1.size(); ++i) {
                                    vch1[i] ^= vch2[i];
                                }
                                break;
                            default:
                                break;
                        }

                        // And pop vch2.
                        popstack(stack);
                    } break;

                    case OP_INVERT: {
                        // (x -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype &vch1 = stacktop(-1);
                        // To avoid allocating, we modify vch1 in place
                        for(size_t i=0; i<vch1.size(); i++)
                        {
                            vch1[i] = ~vch1[i];
                        }
                    } break;

                    /*
                    // Cannot implement until we have proper Big int support

                    case OP_LSHIFT: {
                        // (x n -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        const valtype vch1 = stacktop(-2);
                        CScriptNum n(stacktop(-1), fRequireMinimal, maxIntegerSize);
                        if (n < 0) {
                            return set_error(serror, ScriptError::INVALID_NUMBER_RANGE);
                        }

                        popstack(stack);
                        popstack(stack);
                        stack.push_back(LShift(vch1, n.getint()));
                    } break;

                    case OP_RSHIFT: {
                        // (x n -- out)
                        if (stack.size() < 2) {
                            return set_error( serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        const valtype vch1 = stacktop(-2);
                        CScriptNum n(stacktop(-1), fRequireMinimal, maxIntegerSize);
                        if (n < 0) {
                            return set_error(serror, ScriptError::INVALID_NUMBER_RANGE);
                        }

                        popstack(stack);
                        popstack(stack);
                        stack.push_back(RShift(vch1, n.getint()));
                    } break; */

                    case OP_EQUAL:
                    case OP_EQUALVERIFY:
                        // case OP_NOTEQUAL: // use OP_NUMNOTEQUAL
                        {
                            // (x1 x2 - bool)
                            if (stack.size() < 2) {
                                return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                            }
                            valtype &vch1 = stacktop(-2);
                            valtype &vch2 = stacktop(-1);
                            
                            bool fEqual = (vch1 == vch2);
                            // OP_NOTEQUAL is disabled because it would be too
                            // easy to say something like n != 1 and have some
                            // wiseguy pass in 1 with extra zero bytes after it
                            // (numerically, 0x01 == 0x0001 == 0x000001)
                            // if (opcode == OP_NOTEQUAL)
                            //    fEqual = !fEqual;
                            popstack(stack);
                            popstack(stack);
                            stack.push_back(fEqual ? vchTrue : vchFalse);
                            if (opcode == OP_EQUALVERIFY) {
                                if (fEqual) {
                                    popstack(stack);
                                } else {
                                    return set_error(serror, ScriptError::EQUALVERIFY);
                                }
                            }
                        }
                        break;

                    //
                    // Numeric
                    //
                    case OP_1ADD:
                    case OP_1SUB:
                    case OP_NEGATE:
                    case OP_ABS:
                    case OP_NOT:
                    case OP_0NOTEQUAL: {
                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        CScriptNum bn(stacktop(-1), fRequireMinimal, maxIntegerSize);

                        switch (opcode) {
                            case OP_1ADD: {
                                auto res = bn.safeAdd(1);
                                if ( ! res) {
                                    return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                                }
                                bn = *res;
                                break;
                            }
                            case OP_1SUB: {
                                auto res = bn.safeSub(1);
                                if ( ! res) {
                                    return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                                }
                                bn = *res;
                                break;
                            }
                            case OP_NEGATE:
                                bn = -bn;
                                break;
                            case OP_ABS:
                                if (bn < bnZero) {
                                    bn = -bn;
                                }
                                break;
                            case OP_NOT:
                                bn = CScriptNum::fromIntUnchecked(bn == bnZero);
                                break;
                            case OP_0NOTEQUAL:
                                bn = CScriptNum::fromIntUnchecked(bn != bnZero);
                                break;
                            default:
                                assert(!"invalid opcode");
                                break;
                        }
                        popstack(stack);
                        stack.push_back(bn.getvch());
                    } break;

                    case OP_ADD:
                    case OP_SUB:
                    case OP_MUL:
                    case OP_DIV:
                    case OP_MOD:
                    case OP_BOOLAND:
                    case OP_BOOLOR:
                    case OP_NUMEQUAL:
                    case OP_NUMEQUALVERIFY:
                    case OP_NUMNOTEQUAL:
                    case OP_LESSTHAN:
                    case OP_GREATERTHAN:
                    case OP_LESSTHANOREQUAL:
                    case OP_GREATERTHANOREQUAL:
                    case OP_MIN:
                    case OP_MAX: {
                        // (x1 x2 -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        CScriptNum bn1(stacktop(-2), fRequireMinimal, maxIntegerSize);
                        CScriptNum bn2(stacktop(-1), fRequireMinimal, maxIntegerSize);
                        auto bn = CScriptNum::fromIntUnchecked(0);

                        switch (opcode) {
                            case OP_ADD: {
                                auto res = bn1.safeAdd(bn2);
                                if ( ! res) {
                                    return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                                }
                                bn = *res;
                                break;
                            }

                            case OP_SUB: {
                                auto res = bn1.safeSub(bn2);
                                if ( ! res) {
                                    return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                                }
                                bn = *res;
                                break;
                            }

                            case OP_MUL: {
                                auto res = bn1.safeMul(bn2);
                                if ( ! res) {
                                    return set_error(serror, ScriptError::INVALID_NUMBER_RANGE_64_BIT);
                                }
                                bn = *res;
                                break;
                            }

                            case OP_DIV:
                                // denominator must not be 0
                                if (bn2 == 0) {
                                    return set_error(serror, ScriptError::DIV_BY_ZERO);
                                }
                                bn = bn1 / bn2;
                                break;

                            case OP_MOD:
                                // divisor must not be 0
                                if (bn2 == 0) {
                                    return set_error(serror, ScriptError::MOD_BY_ZERO);
                                }
                                bn = bn1 % bn2;
                                break;

                            case OP_BOOLAND:
                                bn = CScriptNum::fromIntUnchecked(bn1 != bnZero && bn2 != bnZero);
                                break;
                            case OP_BOOLOR:
                                bn = CScriptNum::fromIntUnchecked(bn1 != bnZero || bn2 != bnZero);
                                break;
                            case OP_NUMEQUAL:
                                bn = CScriptNum::fromIntUnchecked(bn1 == bn2);
                                break;
                            case OP_NUMEQUALVERIFY:
                                bn = CScriptNum::fromIntUnchecked(bn1 == bn2);
                                break;
                            case OP_NUMNOTEQUAL:
                                bn = CScriptNum::fromIntUnchecked(bn1 != bn2);
                                break;
                            case OP_LESSTHAN:
                                bn = CScriptNum::fromIntUnchecked(bn1 < bn2);
                                break;
                            case OP_GREATERTHAN:
                                bn = CScriptNum::fromIntUnchecked(bn1 > bn2);
                                break;
                            case OP_LESSTHANOREQUAL:
                                bn = CScriptNum::fromIntUnchecked(bn1 <= bn2);
                                break;
                            case OP_GREATERTHANOREQUAL:
                                bn = CScriptNum::fromIntUnchecked(bn1 >= bn2);
                                break;
                            case OP_MIN:
                                bn = (bn1 < bn2 ? bn1 : bn2);
                                break;
                            case OP_MAX:
                                bn = (bn1 > bn2 ? bn1 : bn2);
                                break;
                            default:
                                assert(!"invalid opcode");
                                break;
                        }
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(bn.getvch());

                        if (opcode == OP_NUMEQUALVERIFY) {
                            if (CastToBool(stacktop(-1))) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::NUMEQUALVERIFY);
                            }
                        }
                    } break;

                    case OP_WITHIN: {
                        // (x min max -- out)
                        if (stack.size() < 3) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        CScriptNum bn1(stacktop(-3), fRequireMinimal, maxIntegerSize);
                        CScriptNum bn2(stacktop(-2), fRequireMinimal, maxIntegerSize);
                        CScriptNum bn3(stacktop(-1), fRequireMinimal, maxIntegerSize);

                        bool fValue = (bn2 <= bn1 && bn1 < bn3);
                        popstack(stack);
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fValue ? vchTrue : vchFalse);
                    } break;

                    //
                    // Crypto
                    //
                    case OP_RIPEMD160:
                    case OP_SHA1:
                    case OP_SHA256:
                    case OP_HASH160:
                    case OP_HASH256:
                    case OP_SHA512_256:
                    case OP_HASH512_256: {
                        // (in -- hash)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype &vch = stacktop(-1);
                        valtype vchHash((opcode == OP_RIPEMD160 ||
                                         opcode == OP_SHA1 ||
                                         opcode == OP_HASH160)
                                            ? 20
                                            : 32);
                        if (opcode == OP_RIPEMD160) {
                            CRIPEMD160()
                                .Write(vch.data(), vch.size())
                                .Finalize(vchHash.data());
                        } else if (opcode == OP_SHA1) {
                            CSHA1()
                                .Write(vch.data(), vch.size())
                                .Finalize(vchHash.data());
                        } else if (opcode == OP_SHA256) {
                            CSHA256()
                                .Write(vch.data(), vch.size())
                                .Finalize(vchHash.data());
                        } else if (opcode == OP_HASH160) {
                            CHash160().Write(vch).Finalize(vchHash);
                        } else if (opcode == OP_HASH256) {
                            CHash256().Write(vch).Finalize(vchHash);
                        } else if (opcode == OP_SHA512_256) {
                            CSHA512_256()
                            .Write(vch.data(), vch.size())
                            .Finalize(vchHash.data());
                        } else if (opcode == OP_HASH512_256) {
                            CHash512_256().Write(vch).Finalize(vchHash);
                        }
                        popstack(stack);
                        stack.push_back(vchHash);
                    } break;

                    case OP_CODESEPARATOR: {
                        // Hash starts after the code separator
                        pbegincodehash = pc;
                    } break;

                    case OP_CHECKSIG:
                    case OP_CHECKSIGVERIFY: {
                        // (sig pubkey -- bool)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype const &vchSig = stacktop(-2);
                        valtype const &vchPubKey = stacktop(-1);

                        if (!CheckTransactionSignatureEncoding(vchSig, flags,
                                                               serror) ||
                            !CheckPubKeyEncoding(vchPubKey, flags, serror)) {
                            // serror is set
                            return false;
                        }

                        bool fSuccess = false;
                        if (vchSig.size()) {
                            // Subset of script starting at the most recent
                            // codeseparator
                            CScript scriptCode(pbegincodehash, pend);

                            fSuccess = checker.CheckSig(vchSig, vchPubKey,
                                                        scriptCode, flags);
                            metrics.nSigChecks += 1;

                            if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL)) {
                                return set_error(serror, ScriptError::SIG_NULLFAIL);
                            }
                        }

                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        if (opcode == OP_CHECKSIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::CHECKSIGVERIFY);
                            }
                        }
                    } break;

                    case OP_CHECKDATASIG:
                    case OP_CHECKDATASIGVERIFY: {
                        // (sig message pubkey -- bool)
                        if (stack.size() < 3) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype const &vchSig = stacktop(-3);
                        valtype const &vchMessage = stacktop(-2);
                        valtype const &vchPubKey = stacktop(-1);

                        if (!CheckDataSignatureEncoding(vchSig, flags,
                                                        serror) ||
                            !CheckPubKeyEncoding(vchPubKey, flags, serror)) {
                            // serror is set
                            return false;
                        }

                        bool fSuccess = false;
                        if (vchSig.size()) {
                            valtype vchHash(32);
                            CSHA256()
                                .Write(vchMessage.data(), vchMessage.size())
                                .Finalize(vchHash.data());
                            fSuccess = checker.VerifySignature(
                                vchSig, CPubKey(vchPubKey), uint256(vchHash));
                            metrics.nSigChecks += 1;

                            if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL)) {
                                return set_error(serror, ScriptError::SIG_NULLFAIL);
                            }
                        }

                        popstack(stack);
                        popstack(stack);
                        popstack(stack);
                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        if (opcode == OP_CHECKDATASIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::CHECKDATASIGVERIFY);
                            }
                        }
                    } break;

                    case OP_CHECKMULTISIG:
                    case OP_CHECKMULTISIGVERIFY: {
                        // ([dummy] [sig ...] num_of_signatures [pubkey ...]
                        // num_of_pubkeys -- bool)
                        const size_t idxKeyCount = 1;
                        if (stack.size() < idxKeyCount) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        int64_t const nKeysCount = CScriptNum(stacktop(-idxKeyCount), fRequireMinimal, maxIntegerSize).getint64();
                        if (nKeysCount < 0 || nKeysCount > MAX_PUBKEYS_PER_MULTISIG) {
                            return set_error(serror, ScriptError::PUBKEY_COUNT);
                        }
                        nOpCount += nKeysCount;
                        if (nOpCount > MAX_OPS_PER_SCRIPT) {
                            return set_error(serror, ScriptError::OP_COUNT);
                        }

                        // stack depth of the top pubkey
                        const size_t idxTopKey = idxKeyCount + 1;

                        // stack depth of nSigsCount
                        const size_t idxSigCount = idxTopKey + nKeysCount;
                        if (stack.size() < idxSigCount) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        int64_t const nSigsCount = CScriptNum(stacktop(-idxSigCount), fRequireMinimal, maxIntegerSize).getint64();
                        if (nSigsCount < 0 || nSigsCount > nKeysCount) {
                            return set_error(serror, ScriptError::SIG_COUNT);
                        }

                        // stack depth of the top signature
                        const size_t idxTopSig = idxSigCount + 1;

                        // stack depth of the dummy element
                        const size_t idxDummy = idxTopSig + nSigsCount;
                        if (stack.size() < idxDummy) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        // Subset of script starting at the most recent
                        // codeseparator
                        CScript scriptCode(pbegincodehash, pend);

                        // Assuming success is usually a bad idea, but the
                        // schnorr path can only succeed.
                        bool fSuccess = true;

                        if ((flags & SCRIPT_ENABLE_SCHNORR_MULTISIG) &&
                            stacktop(-idxDummy).size() != 0) {
                            // SCHNORR MULTISIG
                            static_assert(
                                MAX_PUBKEYS_PER_MULTISIG < 32,
                                "Schnorr multisig checkbits implementation "
                                "assumes < 32 pubkeys.");
                            uint32_t checkBits = 0;

                            // Dummy element is to be interpreted as a bitfield
                            // that represent which pubkeys should be checked.
                            valtype const &vchDummy = stacktop(-idxDummy);
                            if ( ! DecodeBitfield(vchDummy, nKeysCount, checkBits, serror)) {
                                // serror is set
                                return false;
                            }

                            // The bitfield doesn't set the right number of
                            // signatures.
                            if (countBits(checkBits) != uint32_t(nSigsCount)) {
                                return set_error(serror, ScriptError::INVALID_BIT_COUNT);
                            }

                            const size_t idxBottomKey = idxTopKey + nKeysCount - 1;
                            const size_t idxBottomSig = idxTopSig + nSigsCount - 1;

                            int iKey = 0;
                            for (int iSig = 0; iSig < nSigsCount; iSig++, iKey++) {
                                if ((checkBits >> iKey) == 0) {
                                    // This is a sanity check and should be
                                    // unrecheable.
                                    return set_error(serror, ScriptError::INVALID_BIT_RANGE);
                                }

                                // Find the next suitable key.
                                while (((checkBits >> iKey) & 0x01) == 0) {
                                    iKey++;
                                }

                                if (iKey >= nKeysCount) {
                                    // This is a sanity check and should be
                                    // unrecheable.
                                    return set_error(serror, ScriptError::PUBKEY_COUNT);
                                }

                                // Check the signature.
                                valtype const &vchSig = stacktop(-idxBottomSig + iSig);
                                valtype const &vchPubKey = stacktop(-idxBottomKey + iKey);

                                // Note that only pubkeys associated with a
                                // signature are checked for validity.
                                if ( ! CheckTransactionSchnorrSignatureEncoding(vchSig, flags, serror) ||
                                     ! CheckPubKeyEncoding(vchPubKey, flags, serror)) {
                                    // serror is set
                                    return false;
                                }

                                // Check signature
                                if (!checker.CheckSig(vchSig, vchPubKey, scriptCode, flags)) {
                                    // This can fail if the signature is empty,
                                    // which also is a NULLFAIL error as the
                                    // bitfield should have been null in this
                                    // situation.
                                    return set_error(serror, ScriptError::SIG_NULLFAIL);
                                }

                                // this is guaranteed to execute exactly
                                // nSigsCount times (if not script error)
                                metrics.nSigChecks += 1;
                            }

                            if ((checkBits >> iKey) != 0) {
                                // This is a sanity check and should be
                                // unrecheable.
                                return set_error(serror, ScriptError::INVALID_BIT_COUNT);
                            }
                        } else {
                            // LEGACY MULTISIG (ECDSA / NULL)

                            int nSigsRemaining = nSigsCount;
                            int nKeysRemaining = nKeysCount;
                            while (fSuccess && nSigsRemaining > 0) {
                                valtype const &vchSig = stacktop(
                                    -idxTopSig - (nSigsCount - nSigsRemaining));
                                valtype const &vchPubKey = stacktop(
                                    -idxTopKey - (nKeysCount - nKeysRemaining));

                                // Note how this makes the exact order of
                                // pubkey/signature evaluation distinguishable
                                // by CHECKMULTISIG NOT if the STRICTENC flag is
                                // set. See the script_(in)valid tests for
                                // details.
                                if (!CheckTransactionECDSASignatureEncoding(
                                        vchSig, flags, serror) ||
                                    !CheckPubKeyEncoding(vchPubKey, flags,
                                                         serror)) {
                                    // serror is set
                                    return false;
                                }

                                // Check signature
                                bool fOk = checker.CheckSig(vchSig, vchPubKey,
                                                            scriptCode, flags);

                                if (fOk) {
                                    nSigsRemaining--;
                                }
                                nKeysRemaining--;

                                // If there are more signatures left than keys
                                // left, then too many signatures have failed.
                                // Exit early, without checking any further
                                // signatures.
                                if (nSigsRemaining > nKeysRemaining) {
                                    fSuccess = false;
                                }
                            }

                            bool areAllSignaturesNull = true;
                            for (int i = 0; i < nSigsCount; i++) {
                                if (stacktop(-idxTopSig - i).size()) {
                                    areAllSignaturesNull = false;
                                    break;
                                }
                            }

                            // If the operation failed, we may require that all
                            // signatures must be empty vector
                            if (!fSuccess && (flags & SCRIPT_VERIFY_NULLFAIL) &&
                                !areAllSignaturesNull) {
                                return set_error(serror, ScriptError::SIG_NULLFAIL);
                            }

                            if (!areAllSignaturesNull) {
                                // This is not identical to the number of actual
                                // ECDSA verifies, but, it is an upper bound
                                // that can be easily determined without doing
                                // CPU-intensive checks.
                                metrics.nSigChecks += nKeysCount;
                            }
                        }

                        // Clean up stack of all arguments
                        for (size_t i = 0; i < idxDummy; i++) {
                            popstack(stack);
                        }

                        stack.push_back(fSuccess ? vchTrue : vchFalse);
                        if (opcode == OP_CHECKMULTISIGVERIFY) {
                            if (fSuccess) {
                                popstack(stack);
                            } else {
                                return set_error(serror, ScriptError::CHECKMULTISIGVERIFY);
                            }
                        }
                    } break;

                    //
                    // Byte string operations
                    //
                    case OP_CAT: {
                        // (x1 x2 -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        valtype &vch1 = stacktop(-2);
                        valtype &vch2 = stacktop(-1);
                        if (vch1.size() + vch2.size() >
                            MAX_SCRIPT_ELEMENT_SIZE) {
                            return set_error(serror, ScriptError::PUSH_SIZE);
                        }
                        vch1.insert(vch1.end(), vch2.begin(), vch2.end());
                        popstack(stack);
                    } break;

                    case OP_SPLIT: {
                        // (in position -- x1 x2)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        const valtype &data = stacktop(-2);

                        // Make sure the split point is appropriate.
                        int64_t const position = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                        if (position < 0 || uint64_t(position) > data.size()) {
                            return set_error(serror, ScriptError::INVALID_SPLIT_RANGE);
                        }

                        // Prepare the results in their own buffer as `data` will be invalidated.
                        valtype n1(data.begin(), data.begin() + position);
                        valtype n2(data.begin() + position, data.end());

                        // Replace existing stack values by the new values.
                        stacktop(-2) = std::move(n1);
                        stacktop(-1) = std::move(n2);
                    } break;

                    case OP_REVERSEBYTES: {
                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype &data = stacktop(-1);
                        std::reverse(data.begin(), data.end());
                    } break;

                    //
                    // Conversion operations
                    //
                    case OP_NUM2BIN: {
                        // (in size -- out)
                        if (stack.size() < 2) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        uint64_t const size = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                        if (size > MAX_SCRIPT_ELEMENT_SIZE) {
                            return set_error(serror, ScriptError::PUSH_SIZE);
                        }

                        popstack(stack);
                        valtype &rawnum = stacktop(-1);

                        // Try to see if we can fit that number in the number of byte requested.
                        CScriptNum::MinimallyEncode(rawnum);
                        if (rawnum.size() > size) {
                            // We definitively cannot.
                            return set_error(serror, ScriptError::IMPOSSIBLE_ENCODING);
                        }

                        // We already have an element of the right size, we don't need to do anything.
                        if (rawnum.size() == size) {
                            break;
                        }

                        uint8_t signbit = 0x00;
                        if (rawnum.size() > 0) {
                            signbit = rawnum.back() & 0x80;
                            rawnum[rawnum.size() - 1] &= 0x7f;
                        }

                        rawnum.reserve(size);
                        while (rawnum.size() < size - 1) {
                            rawnum.push_back(0x00);
                        }

                        rawnum.push_back(signbit);
                    } break;

                    case OP_BIN2NUM: {
                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }

                        valtype &n = stacktop(-1);
                        CScriptNum::MinimallyEncode(n);

                        // The resulting number must be a valid number.
                        // Note: IsMinimallyEncoded() here is really just checking if the number is in range.
                        if ( ! CScriptNum::IsMinimallyEncoded(n, maxIntegerSize)) {
                            return set_error(serror, invalidNumberRangeError);
                        }
                    } break;

                    // Native Introspection opcodes (Nullary)
                    case OP_INPUTINDEX:
                    case OP_ACTIVEBYTECODE:
                    case OP_TXVERSION:
                    case OP_TXINPUTCOUNT:
                    case OP_TXOUTPUTCOUNT:
                    case OP_TXLOCKTIME: {
                        if ( ! nativeIntrospection) {
                            return set_error(serror, ScriptError::BAD_OPCODE);
                        }
                        if ( ! context) {
                            return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                        }

                        switch (opcode) {
                            //  Operations
                            case OP_INPUTINDEX: {
                                auto const bn = CScriptNum::fromInt(context->inputIndex()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_ACTIVEBYTECODE: {
                                // Subset of script starting at the most recent code separator (if any)
                                // or the entire script if no code separators are present.
                                if (size_t(script.end() - pbegincodehash) > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.emplace_back(pbegincodehash, script.end());
                            } break;
                            case OP_TXVERSION: {
                                auto const bn = CScriptNum::fromInt(context->tx().nVersion()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_TXINPUTCOUNT: {
                                auto const bn = CScriptNum::fromInt(context->tx().vin().size()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_TXOUTPUTCOUNT: {
                                auto const bn = CScriptNum::fromInt(context->tx().vout().size()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_TXLOCKTIME: {
                                auto const bn = CScriptNum::fromInt(context->tx().nLockTime()).value();
                                stack.push_back(bn.getvch());
                            } break;
                            default: {
                                assert(!"invalid opcode");
                                break;
                            }
                        }
                    } break; // end of Native Introspection opcodes (Nullary)

                    // Native Introspection opcodes (Unary)
                    case OP_UTXOVALUE:
                    case OP_UTXOBYTECODE:
                    case OP_OUTPOINTTXHASH:
                    case OP_OUTPOINTINDEX:
                    case OP_INPUTBYTECODE:
                    case OP_INPUTSEQUENCENUMBER:
                    case OP_OUTPUTVALUE:
                    case OP_OUTPUTBYTECODE: {

                        if ( ! nativeIntrospection) {
                            return set_error(serror, ScriptError::BAD_OPCODE);
                        }
                        if ( ! context) {
                            return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                        }

                        // (in -- out)
                        if (stack.size() < 1) {
                            return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                        }
                        auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                        popstack(stack); // consume element

                        switch (opcode) {
                            case OP_UTXOVALUE: {
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const bn = CScriptNum::fromInt(context->coinAmount(index) / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_UTXOBYTECODE: {
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const& utxoScript = context->coinScriptPubKey(index);
                                if (utxoScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.emplace_back(utxoScript.begin(), utxoScript.end());
                            } break;

                            case OP_OUTPOINTTXHASH: {
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                auto const& input = context->tx().vin()[index];
                                auto const& txid = input.prevout.GetTxId();
                                static_assert(TxId::size() <= MAX_SCRIPT_ELEMENT_SIZE);
                                stack.emplace_back(txid.begin(), txid.end());
                            } break;

                            case OP_OUTPOINTINDEX: {
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                auto const& input = context->tx().vin()[index];
                                auto const bn = CScriptNum::fromInt(input.prevout.GetN()).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_INPUTBYTECODE: {
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                auto const& inputScript = context->scriptSig(index);
                                if (inputScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.emplace_back(inputScript.begin(), inputScript.end());
                            } break;

                            case OP_INPUTSEQUENCENUMBER: {
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                auto const& input = context->tx().vin()[index];
                                auto const bn = CScriptNum::fromInt(input.nSequence).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_OUTPUTVALUE: {
                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
                                auto const& output = context->tx().vout()[index];
                                auto const bn = CScriptNum::fromInt(output.nValue / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;

                            case OP_OUTPUTBYTECODE: {
                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
                                auto const& outputScript = context->tx().vout()[index].scriptPubKey;
                                if (outputScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                stack.emplace_back(outputScript.begin(), outputScript.end());
                            } break;
                            default: {
                                assert(!"invalid opcode");
                                break;
                            }
                        }
                    } break; // end of Native Introspection opcodes (Unary)
                    // Start push reference opcodes
                    case OP_PUSHINPUTREF:
                    case OP_REQUIREINPUTREF:
                    case OP_DISALLOWPUSHINPUTREFSIBLING:
                    case OP_DISALLOWPUSHINPUTREF:
                    case OP_REFHASHDATASUMMARY_UTXO: 
                    case OP_REFHASHDATASUMMARY_OUTPUT:
                    case OP_REFHASHVALUESUM_UTXOS:
                    case OP_REFHASHVALUESUM_OUTPUTS:
                    case OP_PUSHINPUTREFSINGLETON: 
                    case OP_REFTYPE_UTXO:
                    case OP_REFTYPE_OUTPUT:
                    case OP_STATESEPARATOR:
                    case OP_STATESEPARATORINDEX_UTXO:
                    case OP_STATESEPARATORINDEX_OUTPUT:
                    case OP_REFVALUESUM_UTXOS:
                    case OP_REFVALUESUM_OUTPUTS:
                    case OP_REFOUTPUTCOUNT_UTXOS:
                    case OP_REFOUTPUTCOUNT_OUTPUTS:
                    case OP_REFOUTPUTCOUNTZEROVALUED_UTXOS:
                    case OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS:
                    case OP_REFDATASUMMARY_UTXO:
                    case OP_REFDATASUMMARY_OUTPUT:
                    case OP_CODESCRIPTHASHVALUESUM_UTXOS:
                    case OP_CODESCRIPTHASHVALUESUM_OUTPUTS:
                    case OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS:
                    case OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS:
                    case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS:
                    case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS:
                    case OP_CODESCRIPTBYTECODE_UTXO:
                    case OP_CODESCRIPTBYTECODE_OUTPUT:
                    case OP_STATESCRIPTBYTECODE_UTXO:
                    case OP_STATESCRIPTBYTECODE_OUTPUT:
                    case OP_PUSH_TX_STATE:
                    {
                        switch (opcode) {
                            case OP_PUSH_TX_STATE: {
                                if ( ! pushTxState) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                
                                auto const fieldItem = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                // fieldNum 0: Txid
                                // fieldNum 1: total input satoshis/photons
                                // fieldNum 2: total output satoshis/photons
                                if (fieldItem < 0 || fieldItem > 2) {
                                    return set_error(serror, ScriptError::INVALID_TX_STATE_ITEM);
                                }

                                switch (fieldItem) {
                                    case 0: {
                                        // Get the txid based on normal version or txid v3
                                        TxId currentTxId = context->GetTxId();
                                        stack.emplace_back(currentTxId.begin(), currentTxId.end());
                                        break;
                                    }
                                    case 1: {
                                        Amount accumulatedInputValue(Amount::zero());
                                        for (uint64_t i = 0; i < context->tx().vin().size(); i++) {
                                            auto const& inputAmount = context->coinAmount(i);
                                            accumulatedInputValue += inputAmount;
                                        }
                                        auto const bn = CScriptNum::fromInt(accumulatedInputValue / SATOSHI).value();
                                        stack.push_back(bn.getvch());
                                        break;
                                    }
                                    case 2: {
                                        Amount accumulatedOutputValue(Amount::zero());
                                        for (uint64_t i = 0; i < context->tx().vout().size(); i++) {
                                            auto const& output = context->tx().vout()[i];
                                            accumulatedOutputValue += output.nValue;
                                        }
                                        auto const bn = CScriptNum::fromInt(accumulatedOutputValue / SATOSHI).value();
                                        stack.push_back(bn.getvch());
                                        break;
                                    }
                                    default:
                                        return set_error(serror, ScriptError::INVALID_TX_STATE_ITEM);
                                }
                            } break;
                            case OP_PUSHINPUTREF: {
                                if (vchPushValue.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                // When interpretting OP_PUSHINPUTREF, just push to the primary stack
                                // As safety check, ensure that the UTXO being spent does indeed have the OP_PUSHINPUTREF saved in it's ref vector
                                // It should never be the case that the check fails since a UTXO can only be committed with the output color verified
                                stack.push_back(vchPushValue);
                                // As a sanity check we save all the pushrefs, and then cross check them against 
                                // OP_DISALLOWPUSHINPUTREF
                                if (pushTxState) {
                                    uint288 uref(vchPushValue);
                                    foundPushRefs.insert(uref);
                                } else {
                                    // Note: this does not actually work and results in all zeroes
                                    uint288 uref = uint288S(std::string(vchPushValue.begin(), vchPushValue.end()).c_str());
                                    foundPushRefs.insert(uref);
                                }
                            } break;
                            case OP_DISALLOWPUSHINPUTREF: {
                                if (vchPushValue.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                if (pushTxState) {
                                    uint288 uref(vchPushValue);
                                    disallowedRefs.insert(uref);
                                    stack.push_back(vchPushValue);
                                } else {
                                    // Note: this does not actually work and results in all zeroes
                                    uint288 uref = uint288S(std::string(vchPushValue.begin(), vchPushValue.end()).c_str());
                                    disallowedRefs.insert(uref);
                                    stack.push_back(vchPushValue);
                                }
                            } break;
                            case OP_DISALLOWPUSHINPUTREFSIBLING: {
                                if (vchPushValue.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                // When interpreting OP_DISALLOWPUSHINPUTREFSIBLING, push the value to the stack
                                stack.push_back(vchPushValue);
                            } break;
                            case OP_REQUIREINPUTREF: {
                                if (vchPushValue.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                // When interpreting OP_REQUIREINPUTREF, push the value to the stack
                                stack.push_back(vchPushValue);
                            } break;
                            case OP_REFHASHDATASUMMARY_UTXO: {
                                // Push a hash256 of the output being spent of a vector of the form:
                                // <nValue><hash256(scriptPubKey)><numRefs><hash(sortedMap(pushRefs))>
                                // This allows an unlocking context to access any other input's scriptPubKey
                                // and determine what 'type' it is and all other details of that script
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const& dataHash = context->getRefHashDataSummaryUtxo(index);
                                stack.emplace_back(dataHash.begin(), dataHash.end());
                            } break;
                            case OP_REFHASHDATASUMMARY_OUTPUT: {
                                // Push a hash256 of an output of a vector of the form:
                                // <nValue><hash256(scriptPubKey)><numRefs><hash(sortedMap(pushRefs))>
                                // This allows the script to access any other output scriptPubKey
                                // and determine what 'type' it is and all other details of that script
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
                                auto const& dataHash = context->getRefHashDataSummaryOutput(index);
                                stack.emplace_back(dataHash.begin(), dataHash.end());

                            } break;
                            case OP_REFHASHVALUESUM_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refHash = stacktop(-1);

                                if (refHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_REFHASH_SIZE);
                                }
                                popstack(stack); // consume element

                                uint256 refHashUint256(refHash);
                                auto const& sumAmount = context->getRefHashValueSumUtxos(refHashUint256);
                                auto bn = CScriptNum::fromInt(sumAmount / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFHASHVALUESUM_OUTPUTS: {
                                if ( ! enhancedReferences) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refHash = stacktop(-1);

                                if (refHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_REFHASH_SIZE);
                                }
                                popstack(stack); // consume element

                                uint256 refHashUint256(refHash);
                                auto const& sumAmount = context->getRefHashValueSumOutputs(refHashUint256);
                                auto bn = CScriptNum::fromInt(sumAmount / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFVALUESUM_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element

                                uint288 refAssetIdUint288(refAssetId);
                                auto const& sumAmount = context->getRefValueSumUtxos(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(sumAmount / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;
                            
                            case OP_REFVALUESUM_OUTPUTS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element

                                uint288 refAssetIdUint288(refAssetId);
                                auto const& sumAmount = context->getRefValueSumOutputs(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(sumAmount / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break; 
                            case OP_PUSHINPUTREFSINGLETON: {
                                if ( ! enhancedReferences) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if (vchPushValue.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                // When interpreting OP_PUSHINPUTREFSINGLETON, push the value to the stack
                                stack.push_back(vchPushValue);
                            } break;

                            case OP_STATESEPARATOR: {
                                if ( ! enhancedReferences) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                // When interpreting OP_STATESEPARATOR, do nothing (NOP)
                            } break;

                            case OP_REFTYPE_UTXO: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element
                                uint288 refAssetIdUint288(refAssetId);
                                auto const& intType = context->getRefTypeUtxo(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFTYPE_OUTPUT: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element
                                uint288 refAssetIdUint288(refAssetId);
                                auto const& intType = context->getRefTypeOutput(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                             } break;
                            case OP_STATESEPARATORINDEX_UTXO: {
                                if ( ! enhancedReferences) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const& intType = context->getStateSeperatorIndexUtxo(index);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_STATESEPARATORINDEX_OUTPUT: {
                                if ( ! enhancedReferences) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const& intType = context->getStateSeperatorIndexOutput(index);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFOUTPUTCOUNT_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element
                                uint288 refAssetIdUint288(refAssetId);
                                auto const& intType = context->getRefOutputCountUtxos(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFOUTPUTCOUNT_OUTPUTS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element
                                uint288 refAssetIdUint288(refAssetId);
                                auto const& intType = context->getRefOutputCountOutputs(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFOUTPUTCOUNTZEROVALUED_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element
                                uint288 refAssetIdUint288(refAssetId);
                                auto const& intType = context->getRefOutputZeroValuedCountUtxos(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype refAssetId = stacktop(-1);

                                if (refAssetId.size() != 36) {
                                    return set_error(serror, ScriptError::INVALID_TX_REF_SIZE);
                                }
                                popstack(stack); // consume element
                                uint288 refAssetIdUint288(refAssetId);
                                auto const& intType = context->getRefOutputZeroValuedCountOutputs(refAssetIdUint288);
                                auto bn = CScriptNum::fromInt(intType).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_REFDATASUMMARY_UTXO: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                std::vector<uint8_t> concatVec;
                                auto const hasAtLeastOneValidRef = context->getRefsPerUtxo(index, concatVec);
                                if (hasAtLeastOneValidRef) {
                                    stack.emplace_back(concatVec.begin(), concatVec.end());
                                } else {
                                    auto bn = CScriptNum::fromInt(0).value();
                                    stack.push_back(bn.getvch());
                                }
                            } break;
                            case OP_REFDATASUMMARY_OUTPUT: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                
                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
    
                                std::vector<uint8_t> concatVec;
                                auto const hasAtLeastOneValidRef = context->getRefsPerOutput(index, concatVec);
                                if (hasAtLeastOneValidRef) {
                                    stack.emplace_back(concatVec.begin(), concatVec.end());
                                } else {
                                    auto bn = CScriptNum::fromInt(0).value();
                                    stack.push_back(bn.getvch());
                                }
                            } break;
                            case OP_CODESCRIPTHASHVALUESUM_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype codeScriptHash = stacktop(-1);

                                if (codeScriptHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_HASH_SIZE);
                                }
                                popstack(stack); // consume element
                                uint256 codeScriptHashUint256(codeScriptHash);
                                auto const& sumAmount = context->getCodeScriptHashValueSumUtxos(codeScriptHashUint256);
                                auto bn = CScriptNum::fromInt(sumAmount / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_CODESCRIPTHASHVALUESUM_OUTPUTS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype codeScriptHash = stacktop(-1);

                                if (codeScriptHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_HASH_SIZE);
                                }
                                popstack(stack); // consume element
                                uint256 codeScriptHashUint256(codeScriptHash);
                                auto const& sumAmount = context->getCodeScriptHashValueSumOutputs(codeScriptHashUint256);
                                auto bn = CScriptNum::fromInt(sumAmount / SATOSHI).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype codeScriptHash = stacktop(-1);

                                if (codeScriptHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_HASH_SIZE);
                                }
                                popstack(stack); // consume element
                                uint256 codeScriptHashUint256(codeScriptHash);
                                auto const& counter = context->getCodeScriptHashOutputCountUtxos(codeScriptHashUint256);
                                auto bn = CScriptNum::fromInt(counter).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype codeScriptHash = stacktop(-1);

                                if (codeScriptHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_HASH_SIZE);
                                }
                                popstack(stack); // consume element
                                uint256 codeScriptHashUint256(codeScriptHash);
                                auto const& counter = context->getCodeScriptHashOutputCountOutputs(codeScriptHashUint256);
                                auto bn = CScriptNum::fromInt(counter).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype codeScriptHash = stacktop(-1);

                                if (codeScriptHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_HASH_SIZE);
                                }
                                popstack(stack); // consume element
                                uint256 codeScriptHashUint256(codeScriptHash);
                                auto const& counter = context->getCodeScriptHashOutputZeroValuedCountUtxos(codeScriptHashUint256);
                                auto bn = CScriptNum::fromInt(counter).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                valtype codeScriptHash = stacktop(-1);

                                if (codeScriptHash.size() != 32) {
                                    return set_error(serror, ScriptError::INVALID_TX_HASH_SIZE);
                                }
                                popstack(stack); // consume element
                                uint256 codeScriptHashUint256(codeScriptHash);
                                auto const& counter = context->getCodeScriptHashOutputZeroValuedCountOutputs(codeScriptHashUint256);
                                auto bn = CScriptNum::fromInt(counter).value();
                                stack.push_back(bn.getvch());
                            } break;
                            case OP_CODESCRIPTBYTECODE_UTXO: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
 
                                auto const& utxoScript = context->coinScriptPubKey(index);
                                if (utxoScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }

                                auto const stateSeperatorIndex = context->getStateSeparatorByteIndexUtxo(index);
                                stack.emplace_back(utxoScript.begin() + stateSeperatorIndex, utxoScript.end());
                            } break;
                            case OP_CODESCRIPTBYTECODE_OUTPUT: {
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }
                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element
                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
                                auto const& outputScript = context->tx().vout()[index].scriptPubKey;
                                if (outputScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                auto const stateSeperatorIndex = context->getStateSeparatorByteIndexOutput(index);
                                stack.emplace_back(outputScript.begin() + stateSeperatorIndex, outputScript.end());
                            } break;
 
                            case OP_STATESCRIPTBYTECODE_UTXO: {

                                 if ( ! nativeIntrospection) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }

                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element

                                if (index < 0 || uint64_t(index) >= context->tx().vin().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_INPUT_INDEX);
                                }
                                if (context->isLimited() && uint64_t(index) != context->inputIndex()) {
                                    // This branch can only happen in tests or other non-consensus code
                                    // that calls the VM without all the *other* inputs' coins.
                                    return set_error(serror, ScriptError::LIMITED_CONTEXT_NO_SIBLING_INFO);
                                }
                                auto const& utxoScript = context->coinScriptPubKey(index);
                                if (utxoScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                auto const stateSeperatorIndex = context->getStateSeparatorByteIndexUtxo(index);

                                if (stateSeperatorIndex > 0) {
                                    stack.emplace_back(utxoScript.begin(), utxoScript.begin() + stateSeperatorIndex - 1); // Do not include the state seperator itself
                                } else {
                                    auto const bn = CScriptNum::fromIntUnchecked(0);
                                    stack.push_back(bn.getvch());   
                                }
                               
                             
                            } break;

                            case OP_STATESCRIPTBYTECODE_OUTPUT: {

                                 if ( ! nativeIntrospection) {
                                    return set_error(serror, ScriptError::BAD_OPCODE);
                                }
                                if ( ! context) {
                                    return set_error(serror, ScriptError::CONTEXT_NOT_PRESENT);
                                }

                                // (in -- out)
                                if (stack.size() < 1) {
                                    return set_error(serror, ScriptError::INVALID_STACK_OPERATION);
                                }
                                auto const index = CScriptNum(stacktop(-1), fRequireMinimal, maxIntegerSize).getint64();
                                popstack(stack); // consume element

                                if (index < 0 || uint64_t(index) >= context->tx().vout().size()) {
                                    return set_error(serror, ScriptError::INVALID_TX_OUTPUT_INDEX);
                                }
                                auto const& outputScript = context->tx().vout()[index].scriptPubKey;
                                if (outputScript.size() > MAX_SCRIPT_ELEMENT_SIZE) {
                                    return set_error(serror, ScriptError::PUSH_SIZE);
                                }
                                auto const stateSeperatorIndex = context->getStateSeparatorByteIndexOutput(index);
                                if (stateSeperatorIndex > 0) {
                                    stack.emplace_back(outputScript.begin(), outputScript.begin() + stateSeperatorIndex - 1); // Do not include the state seperator itself
                                } else {
                                    auto const bn = CScriptNum::fromIntUnchecked(0);
                                    stack.push_back(bn.getvch());
                                }
                            } break;

                            default: {
                                assert(!"invalid push opcode");
                                break;
                            }
                        }
                    } break; // end of RADIANT based induction and introspection op codes


                    default:
                        return set_error(serror, ScriptError::BAD_OPCODE);
                }
            }

            // Size limits
            if (stack.size() + altstack.size() > MAX_STACK_SIZE) {
                return set_error(serror, ScriptError::STACK_SIZE);
            }
        }
    } catch (...) {
        return set_error(serror, ScriptError::UNKNOWN);
    }

    if (!vfExec.empty()) {
        return set_error(serror, ScriptError::UNBALANCED_CONDITIONAL);
    }

    // Verify that none of the prohibit refs appear in the ref set
    // Save the set difference into resultSet
    std::set<uint288> intersectSet;
    // Get an insert iterator to be able to add to the resultSet container
    std::insert_iterator< std::set<uint288> > insertIter (intersectSet, intersectSet.begin());
    std::set_intersection(disallowedRefs.begin(), disallowedRefs.end(), foundPushRefs.begin(), foundPushRefs.end(), insertIter);
    // The rule is fulfilled if there are no disallowed references appearing anywhere
    // There should be none of the disallowed refs
    if (intersectSet.size() > 0) {
        return set_error(serror, ScriptError::INVALID_TX_OUTPUT_CONTAINS_DISALLOWED_PUSHREF);
    }
    return set_success(serror);
}

namespace {

/**
 * Wrapper that serializes like CTransaction, but with the modifications
 *  required for the signature hash done in-place
 */
template <class T> class CTransactionSignatureSerializer {
private:
    //! reference to the spending transaction (the one being serialized)
    const T &txTo;
    //! output script being consumed
    const CScript &scriptCode;
    //! input index of txTo being signed
    const unsigned int nIn;
    //! container for hashtype flags
    const SigHashType sigHashType;

public:
    CTransactionSignatureSerializer(const T &txToIn,
                                    const CScript &scriptCodeIn,
                                    unsigned int nInIn,
                                    SigHashType sigHashTypeIn)
        : txTo(txToIn), scriptCode(scriptCodeIn), nIn(nInIn),
          sigHashType(sigHashTypeIn) {}

    /** Serialize the passed scriptCode, skipping OP_CODESEPARATORs */
    template <typename S> void SerializeScriptCode(S &s) const {
        CScript::const_iterator it = scriptCode.begin();
        CScript::const_iterator itBegin = it;
        opcodetype opcode;
        unsigned int nCodeSeparators = 0;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                nCodeSeparators++;
            }
        }
        ::WriteCompactSize(s, scriptCode.size() - nCodeSeparators);
        it = itBegin;
        while (scriptCode.GetOp(it, opcode)) {
            if (opcode == OP_CODESEPARATOR) {
                s.write((char *)&itBegin[0], it - itBegin - 1);
                itBegin = it;
            }
        }
        if (itBegin != scriptCode.end()) {
            s.write((char *)&itBegin[0], it - itBegin);
        }
    }

    /** Serialize an input of txTo */
    template <typename S> void SerializeInput(S &s, unsigned int nInput) const {
        // In case of SIGHASH_ANYONECANPAY, only the input being signed is
        // serialized
        if (sigHashType.hasAnyoneCanPay()) {
            nInput = nIn;
        }
        // Serialize the prevout
        ::Serialize(s, txTo.vin[nInput].prevout);
        // Serialize the script
        if (nInput != nIn) {
            // Blank out other inputs' signatures
            ::Serialize(s, CScript());
        } else {
            SerializeScriptCode(s);
        }
        // Serialize the nSequence
        if (nInput != nIn &&
            (sigHashType.getBaseType() == BaseSigHashType::SINGLE ||
             sigHashType.getBaseType() == BaseSigHashType::NONE)) {
            // let the others update at will
            ::Serialize(s, (int)0);
        } else {
            ::Serialize(s, txTo.vin[nInput].nSequence);
        }
    }

    /** Serialize an output of txTo */
    template <typename S>
    void SerializeOutput(S &s, unsigned int nOutput) const {
        if (sigHashType.getBaseType() == BaseSigHashType::SINGLE &&
            nOutput != nIn) {
            // Do not lock-in the txout payee at other indices as txin
            ::Serialize(s, CTxOut());
        } else {
            ::Serialize(s, txTo.vout[nOutput]);
        }
    }

    /** Serialize txTo */
    template <typename S> void Serialize(S &s) const {
        // Serialize nVersion
        ::Serialize(s, txTo.nVersion);
        // Serialize vin
        unsigned int nInputs =
            sigHashType.hasAnyoneCanPay() ? 1 : txTo.vin.size();
        ::WriteCompactSize(s, nInputs);
        for (unsigned int nInput = 0; nInput < nInputs; nInput++) {
            SerializeInput(s, nInput);
        }
        // Serialize vout
        unsigned int nOutputs =
            (sigHashType.getBaseType() == BaseSigHashType::NONE)
                ? 0
                : ((sigHashType.getBaseType() == BaseSigHashType::SINGLE)
                       ? nIn + 1
                       : txTo.vout.size());
        ::WriteCompactSize(s, nOutputs);
        for (unsigned int nOutput = 0; nOutput < nOutputs; nOutput++) {
            SerializeOutput(s, nOutput);
        }
        // Serialize nLockTime
        ::Serialize(s, txTo.nLockTime);
    }
};

template <class T> uint256 GetPrevoutHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : txTo.vin) {
        ss << txin.prevout;
    }
    return ss.GetHash();
}

template <class T> uint256 GetSequenceHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}

template <class T> uint256 GetOutputsHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txout : txTo.vout) {
        ss << txout;
    }
    return ss.GetHash();
}

} // namespace

template <class T>
PrecomputedTransactionData::PrecomputedTransactionData(const T &txTo) {
    hashPrevouts = GetPrevoutHash(txTo);
    hashSequence = GetSequenceHash(txTo);
    hashOutputs = GetOutputsHash(txTo);
    hashOutputHashes = GetHashOutputHashes(txTo);
}

// explicit instantiation
template PrecomputedTransactionData::PrecomputedTransactionData(
    const CTransaction &txTo);
template PrecomputedTransactionData::PrecomputedTransactionData(
    const CMutableTransaction &txTo);

template <class T>
uint256 SignatureHash(const CScript &scriptCode, const T &txTo,
                      unsigned int nIn, SigHashType sigHashType,
                      const Amount amount,
                      const PrecomputedTransactionData *cache, uint32_t flags) {
    assert(nIn < txTo.vin.size());

    if (sigHashType.hasForkId() && (flags & SCRIPT_ENABLE_SIGHASH_FORKID)) {
        uint256 hashPrevouts;
        uint256 hashSequence;
        uint256 hashOutputs;
        uint256 hashOutputHashes;

        if (!sigHashType.hasAnyoneCanPay()) {
            hashPrevouts = cache ? cache->hashPrevouts : GetPrevoutHash(txTo);
        }

        if (!sigHashType.hasAnyoneCanPay() &&
            (sigHashType.getBaseType() != BaseSigHashType::SINGLE) &&
            (sigHashType.getBaseType() != BaseSigHashType::NONE)) {
            hashSequence = cache ? cache->hashSequence : GetSequenceHash(txTo);
        }

        if ((sigHashType.getBaseType() != BaseSigHashType::SINGLE) &&
            (sigHashType.getBaseType() != BaseSigHashType::NONE)) {
            hashOutputs = cache ? cache->hashOutputs : GetOutputsHash(txTo);
            hashOutputHashes = cache ? cache->hashOutputHashes : GetHashOutputHashes(txTo);
        } else if ((sigHashType.getBaseType() == BaseSigHashType::SINGLE) &&
                   (nIn < txTo.vout.size())) {
            CHashWriter hashOutputSs(SER_GETHASH, 0);
            hashOutputSs << txTo.vout[nIn];
            hashOutputs = hashOutputSs.GetHash();

            CHashWriter hashOutputHashesSs(SER_GETHASH, 0);
            uint256 zeroRefHash(uint256S("0000000000000000000000000000000000000000000000000000000000000000"));
            writeOutputDataSummaryVector(hashOutputHashesSs, txTo.vout[nIn].scriptPubKey, txTo.vout[nIn].nValue, zeroRefHash);
            hashOutputHashes = hashOutputHashesSs.GetHash();
        }

        CHashWriter ss(SER_GETHASH, 0);
        // Version
        ss << txTo.nVersion;
        // Input prevouts/nSequence (none/all, depending on flags)
        ss << hashPrevouts;
        ss << hashSequence;
        // The input being signed (replacing the scriptSig with scriptCode +
        // amount). The prevout may already be contained in hashPrevout, and the
        // nSequence may already be contain in hashSequence.
        ss << txTo.vin[nIn].prevout;
        ss << scriptCode;
        ss << amount;
        ss << txTo.vin[nIn].nSequence;
        // Output Hashes(none/one/all, depending on flags)
        ss << hashOutputHashes;
         // Outputs (none/one/all, depending on flags)
        ss << hashOutputs;  // Still maintain the regular hashOutputs for compatibility with other smart contracts and also ease of use
        // Locktime
        ss << txTo.nLockTime;
        // Sighash type
        ss << sigHashType;

        return ss.GetHash();
    }

    static const uint256 one(uint256S(
        "0000000000000000000000000000000000000000000000000000000000000001"));

    // Check for invalid use of SIGHASH_SINGLE
    if ((sigHashType.getBaseType() == BaseSigHashType::SINGLE) &&
        (nIn >= txTo.vout.size())) {
        //  nOut out of range
        return one;
    }

    // Wrapper to serialize only the necessary parts of the transaction being
    // signed
    CTransactionSignatureSerializer<T> txTmp(txTo, scriptCode, nIn,
                                             sigHashType);

    // Serialize and hash
    CHashWriter ss(SER_GETHASH, 0);
    ss << txTmp << sigHashType;
    return ss.GetHash();
}

bool BaseSignatureChecker::VerifySignature(const std::vector<uint8_t> &vchSig,
                                           const CPubKey &pubkey,
                                           const uint256 &sighash) const {
    if (vchSig.size() == 64) {
        return pubkey.VerifySchnorr(sighash, vchSig);
    } else {
        return pubkey.VerifyECDSA(sighash, vchSig);
    }
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckSig(
    const std::vector<uint8_t> &vchSigIn, const std::vector<uint8_t> &vchPubKey,
    const CScript &scriptCode, uint32_t flags) const {
    CPubKey pubkey(vchPubKey);
    if (!pubkey.IsValid()) {
        return false;
    }

    // Hash type is one byte tacked on to the end of the signature
    std::vector<uint8_t> vchSig(vchSigIn);
    if (vchSig.empty()) {
        return false;
    }
    SigHashType sigHashType = GetHashType(vchSig);
    vchSig.pop_back();

    uint256 sighash = SignatureHash(scriptCode, *txTo, nIn, sigHashType, amount,
                                    this->txdata, flags);

    if (!VerifySignature(vchSig, pubkey, sighash)) {
        return false;
    }

    return true;
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckLockTime(
    const CScriptNum &nLockTime) const {
    // There are two kinds of nLockTime: lock-by-blockheight and
    // lock-by-blocktime, distinguished by whether nLockTime <
    // LOCKTIME_THRESHOLD.
    //
    // We want to compare apples to apples, so fail the script unless the type
    // of nLockTime being tested is the same as the nLockTime in the
    // transaction.
    if (!((txTo->nLockTime < LOCKTIME_THRESHOLD &&
           nLockTime < LOCKTIME_THRESHOLD) ||
          (txTo->nLockTime >= LOCKTIME_THRESHOLD &&
           nLockTime >= LOCKTIME_THRESHOLD))) {
        return false;
    }

    // Now that we know we're comparing apples-to-apples, the comparison is a
    // simple numeric one.
    if (nLockTime > int64_t(txTo->nLockTime)) {
        return false;
    }

    // Finally the nLockTime feature can be disabled and thus
    // CHECKLOCKTIMEVERIFY bypassed if every txin has been finalized by setting
    // nSequence to maxint. The transaction would be allowed into the
    // blockchain, making the opcode ineffective.
    //
    // Testing if this vin is not final is sufficient to prevent this condition.
    // Alternatively we could test all inputs, but testing just this input
    // minimizes the data required to prove correct CHECKLOCKTIMEVERIFY
    // execution.
    if (CTxIn::SEQUENCE_FINAL == txTo->vin[nIn].nSequence) {
        return false;
    }

    return true;
}

template <class T>
bool GenericTransactionSignatureChecker<T>::CheckSequence(const CScriptNum &nSequence) const {
    // Relative lock times are supported by comparing the passed in operand to
    // the sequence number of the input.
    const int64_t txToSequence = int64_t(txTo->vin[nIn].nSequence);

    // Fail if the transaction's version number is not set high enough to
    // trigger BIP 68 rules.
    if (static_cast<uint32_t>(txTo->nVersion) < 2) {
        return false;
    }

    // Sequence numbers with their most significant bit set are not consensus
    // constrained. Testing that the transaction's sequence number do not have
    // this bit set prevents using this property to get around a
    // CHECKSEQUENCEVERIFY check.
    if (txToSequence & CTxIn::SEQUENCE_LOCKTIME_DISABLE_FLAG) {
        return false;
    }

    // Mask off any bits that do not have consensus-enforced meaning before
    // doing the integer comparisons
    const uint32_t nLockTimeMask = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | CTxIn::SEQUENCE_LOCKTIME_MASK;
    const int64_t txToSequenceMasked = txToSequence & nLockTimeMask;

    auto const res = nSequence.safeBitwiseAnd(nLockTimeMask);
    if ( ! res) {
        // Defensive programming: It is impossible that this branch be taken unless the current
        // values of the operands are changed.
        return false;
    }
    auto const nSequenceMasked = *res;

    // There are two kinds of nSequence: lock-by-blockheight and
    // lock-by-blocktime, distinguished by whether nSequenceMasked <
    // CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG.
    //
    // We want to compare apples to apples, so fail the script unless the type
    // of nSequenceMasked being tested is the same as the nSequenceMasked in the
    // transaction.
    if (!((txToSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG &&
           nSequenceMasked < CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG) ||
          (txToSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG &&
           nSequenceMasked >= CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG))) {
        return false;
    }

    // Now that we know we're comparing apples-to-apples, the comparison is a
    // simple numeric one.
    if (nSequenceMasked > txToSequenceMasked) {
        return false;
    }

    return true;
}

// explicit instantiation
template class GenericTransactionSignatureChecker<CTransaction>;
template class GenericTransactionSignatureChecker<CMutableTransaction>;

bool VerifyScript(const CScript &scriptSig, const CScript &scriptPubKey, uint32_t flags, const BaseSignatureChecker &checker,
                  ScriptExecutionMetrics &metricsOut, ScriptExecutionContextOpt const& context, ScriptError *serror) {
    set_error(serror, ScriptError::UNKNOWN);

    // If FORKID is enabled, we also ensure strict encoding.
    if (flags & SCRIPT_ENABLE_SIGHASH_FORKID) {
        flags |= SCRIPT_VERIFY_STRICTENC;
    }

    if ((flags & SCRIPT_VERIFY_SIGPUSHONLY) != 0 && !scriptSig.IsPushOnly()) {
        return set_error(serror, ScriptError::SIG_PUSHONLY);
    }
    
    ScriptExecutionMetrics metrics = {};

    std::vector<valtype> stack, stackCopy;
    if ( ! EvalScript(stack, scriptSig, flags, checker, metrics, context, serror)) {
        // serror is set
        return false;
    }
    if (flags & SCRIPT_VERIFY_P2SH) {
        stackCopy = stack;
    }
    if ( ! EvalScript(stack, scriptPubKey, flags, checker, metrics, context, serror)) {
        // serror is set
        return false;
    }
    if (stack.empty()) {
        return set_error(serror, ScriptError::EVAL_FALSE);
    }
    if (CastToBool(stack.back()) == false) {
        return set_error(serror, ScriptError::EVAL_FALSE);
    }

    // Additional validation for spend-to-script-hash transactions:
    if ((flags & SCRIPT_VERIFY_P2SH) && scriptPubKey.IsPayToScriptHash()) {
        // scriptSig must be literals-only or validation fails
        if (!scriptSig.IsPushOnly()) {
            return set_error(serror, ScriptError::SIG_PUSHONLY);
        }

        // Restore stack.
        swap(stack, stackCopy);

        // stack cannot be empty here, because if it was the P2SH  HASH <> EQUAL
        // scriptPubKey would be evaluated with an empty stack and the
        // EvalScript above would return false.
        assert(!stack.empty());

        const valtype &pubKeySerialized = stack.back();
        CScript pubKey2(pubKeySerialized.begin(), pubKeySerialized.end());
        popstack(stack);

        // Bail out early if SCRIPT_DISALLOW_SEGWIT_RECOVERY is not set, the
        // redeem script is a p2sh segwit program, and it was the only item
        // pushed onto the stack.
        if ((flags & SCRIPT_DISALLOW_SEGWIT_RECOVERY) == 0 && stack.empty() &&
            pubKey2.IsWitnessProgram()) {
            // must set metricsOut for all successful returns
            metricsOut = metrics;
            return set_success(serror);
        }

        if ( ! EvalScript(stack, pubKey2, flags, checker, metrics, context, serror)) {
            // serror is set
            // throw std::runtime_error("!EvalScript");
            return false;
        }
        if (stack.empty()) {
             
            // throw std::runtime_error("stack.empty()");
            return set_error(serror, ScriptError::EVAL_FALSE);
        }
        if (!CastToBool(stack.back())) {
            return set_error(serror, ScriptError::EVAL_FALSE);
        }
    }

    // The CLEANSTACK check is only performed after potential P2SH evaluation,
    // as the non-P2SH evaluation of a P2SH script will obviously not result in
    // a clean stack (the P2SH inputs remain). The same holds for witness
    // evaluation.
    if ((flags & SCRIPT_VERIFY_CLEANSTACK) != 0) {
        // Disallow CLEANSTACK without P2SH, as otherwise a switch
        // CLEANSTACK->P2SH+CLEANSTACK would be possible, which is not a
        // softfork (and P2SH should be one).
        

        // Todo: why do tests fail when this is not set and SCRIPT_VERIFY_CLEANSTACK is set?
        // Todo: just remove P2SH entirely
        // assert((flags & SCRIPT_VERIFY_P2SH) != 0);
        if (stack.size() != 1) {
            return set_error(serror, ScriptError::CLEANSTACK);
        }
    }

    /*
    // Removed by Radiant.
    if (flags & SCRIPT_VERIFY_INPUT_SIGCHECKS) {
        // This limit is intended for standard use, and is based on an
        // examination of typical and historical standard uses.
        // - allowing P2SH ECDSA multisig with compressed keys, which at an
        // extreme (1-of-15) may have 15 SigChecks in ~590 bytes of scriptSig.
        // - allowing Bare ECDSA multisig, which at an extreme (1-of-3) may have
        // 3 sigchecks in ~72 bytes of scriptSig.
        // - Since the size of an input is 41 bytes + length of scriptSig, then
        // the most dense possible inputs satisfying this rule would be:
        //   2 sigchecks and 26 bytes: 1/33.50 sigchecks/byte.
        //   3 sigchecks and 69 bytes: 1/36.66 sigchecks/byte.
        // The latter can be readily done with 1-of-3 bare multisignatures,
        // however the former is not practically doable with standard scripts,
        // so the practical density limit is 1/36.66.
        static_assert(INT_MAX > MAX_SCRIPT_SIZE,
                      "overflow sanity check on max script size");
        if (int(scriptSig.size()) < metrics.nSigChecks * 43 - 60) {
            return set_error(serror, ScriptError::INPUT_SIGCHECKS);
        }
    }*/

    metricsOut = metrics;
    return set_success(serror);
}
