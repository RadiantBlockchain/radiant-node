// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/script.h>

#include <script/script_flags.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <iostream>
#include <algorithm>


const char *GetOpName(opcodetype opcode) {
    switch (opcode) {
        // push value
        case OP_0:
            return "0";
        case OP_PUSHDATA1:
            return "OP_PUSHDATA1";
        case OP_PUSHDATA2:
            return "OP_PUSHDATA2";
        case OP_PUSHDATA4:
            return "OP_PUSHDATA4";
        case OP_1NEGATE:
            return "-1";
        case OP_RESERVED:
            return "OP_RESERVED";
        case OP_1:
            return "1";
        case OP_2:
            return "2";
        case OP_3:
            return "3";
        case OP_4:
            return "4";
        case OP_5:
            return "5";
        case OP_6:
            return "6";
        case OP_7:
            return "7";
        case OP_8:
            return "8";
        case OP_9:
            return "9";
        case OP_10:
            return "10";
        case OP_11:
            return "11";
        case OP_12:
            return "12";
        case OP_13:
            return "13";
        case OP_14:
            return "14";
        case OP_15:
            return "15";
        case OP_16:
            return "16";

        // control
        case OP_NOP:
            return "OP_NOP";
        case OP_VER:
            return "OP_VER";
        case OP_IF:
            return "OP_IF";
        case OP_NOTIF:
            return "OP_NOTIF";
        case OP_VERIF:
            return "OP_VERIF";
        case OP_VERNOTIF:
            return "OP_VERNOTIF";
        case OP_ELSE:
            return "OP_ELSE";
        case OP_ENDIF:
            return "OP_ENDIF";
        case OP_VERIFY:
            return "OP_VERIFY";
        case OP_RETURN:
            return "OP_RETURN";

        // stack ops
        case OP_TOALTSTACK:
            return "OP_TOALTSTACK";
        case OP_FROMALTSTACK:
            return "OP_FROMALTSTACK";
        case OP_2DROP:
            return "OP_2DROP";
        case OP_2DUP:
            return "OP_2DUP";
        case OP_3DUP:
            return "OP_3DUP";
        case OP_2OVER:
            return "OP_2OVER";
        case OP_2ROT:
            return "OP_2ROT";
        case OP_2SWAP:
            return "OP_2SWAP";
        case OP_IFDUP:
            return "OP_IFDUP";
        case OP_DEPTH:
            return "OP_DEPTH";
        case OP_DROP:
            return "OP_DROP";
        case OP_DUP:
            return "OP_DUP";
        case OP_NIP:
            return "OP_NIP";
        case OP_OVER:
            return "OP_OVER";
        case OP_PICK:
            return "OP_PICK";
        case OP_ROLL:
            return "OP_ROLL";
        case OP_ROT:
            return "OP_ROT";
        case OP_SWAP:
            return "OP_SWAP";
        case OP_TUCK:
            return "OP_TUCK";

        // splice ops
        case OP_CAT:
            return "OP_CAT";
        case OP_SPLIT:
            return "OP_SPLIT";
        case OP_NUM2BIN:
            return "OP_NUM2BIN";
        case OP_BIN2NUM:
            return "OP_BIN2NUM";
        case OP_SIZE:
            return "OP_SIZE";

        // bit logic
        case OP_INVERT:
            return "OP_INVERT";
        case OP_AND:
            return "OP_AND";
        case OP_OR:
            return "OP_OR";
        case OP_XOR:
            return "OP_XOR";
        case OP_EQUAL:
            return "OP_EQUAL";
        case OP_EQUALVERIFY:
            return "OP_EQUALVERIFY";
        case OP_RESERVED1:
            return "OP_RESERVED1";
        case OP_RESERVED2:
            return "OP_RESERVED2";

        // numeric
        case OP_1ADD:
            return "OP_1ADD";
        case OP_1SUB:
            return "OP_1SUB";
        case OP_2MUL:
            return "OP_2MUL";
        case OP_2DIV:
            return "OP_2DIV";
        case OP_NEGATE:
            return "OP_NEGATE";
        case OP_ABS:
            return "OP_ABS";
        case OP_NOT:
            return "OP_NOT";
        case OP_0NOTEQUAL:
            return "OP_0NOTEQUAL";
        case OP_ADD:
            return "OP_ADD";
        case OP_SUB:
            return "OP_SUB";
        case OP_MUL:
            return "OP_MUL";
        case OP_DIV:
            return "OP_DIV";
        case OP_MOD:
            return "OP_MOD";
        case OP_LSHIFT:
            return "OP_LSHIFT";
        case OP_RSHIFT:
            return "OP_RSHIFT";
        case OP_BOOLAND:
            return "OP_BOOLAND";
        case OP_BOOLOR:
            return "OP_BOOLOR";
        case OP_NUMEQUAL:
            return "OP_NUMEQUAL";
        case OP_NUMEQUALVERIFY:
            return "OP_NUMEQUALVERIFY";
        case OP_NUMNOTEQUAL:
            return "OP_NUMNOTEQUAL";
        case OP_LESSTHAN:
            return "OP_LESSTHAN";
        case OP_GREATERTHAN:
            return "OP_GREATERTHAN";
        case OP_LESSTHANOREQUAL:
            return "OP_LESSTHANOREQUAL";
        case OP_GREATERTHANOREQUAL:
            return "OP_GREATERTHANOREQUAL";
        case OP_MIN:
            return "OP_MIN";
        case OP_MAX:
            return "OP_MAX";
        case OP_WITHIN:
            return "OP_WITHIN";

        // crypto
        case OP_RIPEMD160:
            return "OP_RIPEMD160";
        case OP_SHA1:
            return "OP_SHA1";
        case OP_SHA256:
            return "OP_SHA256";
        case OP_HASH160:
            return "OP_HASH160";
        case OP_HASH256:
            return "OP_HASH256";
        case OP_CODESEPARATOR:
            return "OP_CODESEPARATOR";
        case OP_CHECKSIG:
            return "OP_CHECKSIG";
        case OP_CHECKSIGVERIFY:
            return "OP_CHECKSIGVERIFY";
        case OP_CHECKMULTISIG:
            return "OP_CHECKMULTISIG";
        case OP_CHECKMULTISIGVERIFY:
            return "OP_CHECKMULTISIGVERIFY";
        case OP_CHECKDATASIG:
            return "OP_CHECKDATASIG";
        case OP_CHECKDATASIGVERIFY:
            return "OP_CHECKDATASIGVERIFY";
        case OP_REVERSEBYTES:
            return "OP_REVERSEBYTES";

        // expansion
        case OP_NOP1:
            return "OP_NOP1";
        case OP_CHECKLOCKTIMEVERIFY:
            return "OP_CHECKLOCKTIMEVERIFY";
        case OP_CHECKSEQUENCEVERIFY:
            return "OP_CHECKSEQUENCEVERIFY";
        case OP_NOP4:
            return "OP_NOP4";
        case OP_NOP5:
            return "OP_NOP5";
        case OP_NOP6:
            return "OP_NOP6";
        case OP_NOP7:
            return "OP_NOP7";
        case OP_NOP8:
            return "OP_NOP8";
        case OP_NOP9:
            return "OP_NOP9";
        case OP_NOP10:
            return "OP_NOP10";

        // Native Introspection opcodes
        case OP_INPUTINDEX:
            return "OP_INPUTINDEX";
        case OP_ACTIVEBYTECODE:
            return "OP_ACTIVEBYTECODE";
        case OP_TXVERSION:
            return "OP_TXVERSION";
        case OP_TXINPUTCOUNT:
            return "OP_TXINPUTCOUNT";
        case OP_TXOUTPUTCOUNT:
            return "OP_TXOUTPUTCOUNT";
        case OP_TXLOCKTIME:
            return "OP_TXLOCKTIME";

        case OP_UTXOVALUE:
            return "OP_UTXOVALUE";
        case OP_UTXOBYTECODE:
            return "OP_UTXOBYTECODE";
        case OP_OUTPOINTTXHASH:
            return "OP_OUTPOINTTXHASH";
        case OP_OUTPOINTINDEX:
            return "OP_OUTPOINTINDEX";
        case OP_INPUTBYTECODE:
            return "OP_INPUTBYTECODE";
        case OP_INPUTSEQUENCENUMBER:
            return "OP_INPUTSEQUENCENUMBER";
        case OP_OUTPUTVALUE:
            return "OP_OUTPUTVALUE";
        case OP_OUTPUTBYTECODE:
            return "OP_OUTPUTBYTECODE";
        case OP_SHA512_256:
            return "OP_SHA512_256";
        case OP_HASH512_256:
            return "OP_HASH512_256";
        case OP_PUSHINPUTREF:
            return "OP_PUSHINPUTREF";
        case OP_REQUIREINPUTREF:
            return "OP_REQUIREINPUTREF";
        case OP_DISALLOWPUSHINPUTREF:
            return "OP_DISALLOWPUSHINPUTREF";
        case OP_DISALLOWPUSHINPUTREFSIBLING:
            return "OP_DISALLOWPUSHINPUTREFSIBLING";
        case OP_REFHASHDATASUMMARY_UTXO:
            return "OP_REFHASHDATASUMMARY_UTXO";
        case OP_REFHASHVALUESUM_UTXOS:
            return "OP_REFHASHVALUESUM_UTXOS";
        // SCRIPT_ENHANCED_REFERENCES
        case OP_REFHASHVALUESUM_OUTPUTS:
            return "OP_REFHASHVALUESUM_OUTPUTS";
        case OP_REFHASHDATASUMMARY_OUTPUT:
            return "OP_REFHASHDATASUMMARY_OUTPUT";
        case OP_PUSHINPUTREFSINGLETON:
            return "OP_PUSHINPUTREFSINGLETON";
        case OP_REFTYPE_UTXO:
            return "OP_REFTYPE_UTXO";
        case OP_REFTYPE_OUTPUT:
            return "OP_REFTYPE_OUTPUT";
        case OP_STATESEPARATOR:
            return "OP_STATESEPARATOR";
        case OP_STATESEPARATORINDEX_UTXO:
            return "OP_STATESEPARATORINDEX_UTXO";
        case OP_STATESEPARATORINDEX_OUTPUT:
            return "OP_STATESEPARATORINDEX_OUTPUT";
        case OP_REFVALUESUM_UTXOS:
            return "OP_REFVALUESUM_UTXOS";
        case OP_REFVALUESUM_OUTPUTS:
            return "OP_REFVALUESUM_OUTPUTS";
        case OP_REFOUTPUTCOUNT_UTXOS:
            return "OP_REFOUTPUTCOUNT_UTXOS";
        case OP_REFOUTPUTCOUNT_OUTPUTS:
            return "OP_REFOUTPUTCOUNT_OUTPUTS";
        case OP_REFOUTPUTCOUNTZEROVALUED_UTXOS:
            return "OP_REFOUTPUTCOUNTZEROVALUED_UTXOS";
        case OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS:
            return "OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS";
        case OP_REFDATASUMMARY_UTXO:
            return "OP_REFDATASUMMARY_UTXO";
        case OP_REFDATASUMMARY_OUTPUT:
            return "OP_REFDATASUMMARY_OUTPUT";
        case OP_CODESCRIPTHASHVALUESUM_UTXOS:
            return "OP_CODESCRIPTHASHVALUESUM_UTXOS";
        case OP_CODESCRIPTHASHVALUESUM_OUTPUTS:
            return "OP_CODESCRIPTHASHVALUESUM_OUTPUTS";
        case OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS:
            return "OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS";
        case OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS:
            return "OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS";
        case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS:
            return "OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS";
        case OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS:
            return "OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS";
        case OP_CODESCRIPTBYTECODE_UTXO:
            return "OP_CODESCRIPTBYTECODE_UTXO";
        case OP_CODESCRIPTBYTECODE_OUTPUT:
            return "OP_CODESCRIPTBYTECODE_OUTPUT";
        case OP_STATECRIPTBYTECODE_UTXO:
            return "OP_STATECRIPTBYTECODE_UTXO";
        case OP_STATECRIPTBYTECODE_OUTPUT:
            return "OP_STATECRIPTBYTECODE_OUTPUT";

        // Should we have a way to return a count of the total unique non-zero 
        // references per input and output?
        // It could be used to disallow any inputs
        // case OP_TOTALUNIQUEREFCOUNT_OUTPUTS: todo
        // case OP_TOTALUNIQUEREFCOUNT_INPUTS: todo
        default:
            return "OP_UNKNOWN";
    }
}

bool CheckMinimalPush(const std::vector<uint8_t> &data, opcodetype opcode) {
    // Excludes OP_1NEGATE, OP_1-16 since they are by definition minimal
    assert(0 <= opcode && opcode <= OP_PUSHDATA4);
    if (data.size() == 0) {
        // Should have used OP_0.
        return opcode == OP_0;
    }
    if (data.size() == 1 && data[0] >= 1 && data[0] <= 16) {
        // Should have used OP_1 .. OP_16.
        return false;
    }
    if (data.size() == 1 && data[0] == 0x81) {
        // Should have used OP_1NEGATE.
        return false;
    }
    if (data.size() <= 75) {
        // Must have used a direct push (opcode indicating number of bytes
        // pushed + those bytes).
        return opcode == data.size();
    }
    if (data.size() <= 255) {
        // Must have used OP_PUSHDATA.
        return opcode == OP_PUSHDATA1;
    }
    if (data.size() <= 65535) {
        // Must have used OP_PUSHDATA2.
        return opcode == OP_PUSHDATA2;
    }
    return true;
}

bool CScriptNum::IsMinimallyEncoded(const std::vector<uint8_t> &vch, size_t maxIntegerSize) {
    if (vch.size() > maxIntegerSize) {
        return false;
    }

    if (vch.size() > 0) {
        // Check that the number is encoded with the minimum possible number
        // of bytes.
        //
        // If the most-significant-byte - excluding the sign bit - is zero
        // then we're not minimal. Note how this test also rejects the
        // negative-zero encoding, 0x80.
        if ((vch.back() & 0x7f) == 0) {
            // One exception: if there's more than one byte and the most
            // significant bit of the second-most-significant-byte is set it
            // would conflict with the sign bit. An example of this case is
            // +-255, which encode to 0xff00 and 0xff80 respectively.
            // (big-endian).
            if (vch.size() <= 1 || (vch[vch.size() - 2] & 0x80) == 0) {
                return false;
            }
        }
    }

    return true;
}

bool CScriptNum::MinimallyEncode(std::vector<uint8_t> &data) {
    if (data.size() == 0) {
        return false;
    }

    // If the last byte is not 0x00 or 0x80, we are minimally encoded.
    uint8_t last = data.back();
    if (last & 0x7f) {
        return false;
    }

    // If the script is one byte long, then we have a zero, which encodes as an
    // empty array.
    if (data.size() == 1) {
        data = {};
        return true;
    }

    // If the next byte has it sign bit set, then we are minimaly encoded.
    if (data[data.size() - 2] & 0x80) {
        return false;
    }

    // We are not minimally encoded, we need to figure out how much to trim.
    for (size_t i = data.size() - 1; i > 0; i--) {
        // We found a non zero byte, time to encode.
        if (data[i - 1] != 0) {
            if (data[i - 1] & 0x80) {
                // We found a byte with it sign bit set so we need one more
                // byte.
                data[i++] = last;
            } else {
                // the sign bit is clear, we can use it.
                data[i - 1] |= last;
            }

            data.resize(i);
            return true;
        }
    }

    // If we the whole thing is zeros, then we have a zero.
    data = {};
    return true;
}

bool CScript::IsPayToScriptHash() const {
    // Extra-fast test for pay-to-script-hash CScripts:
    return (this->size() == 23 && (*this)[0] == OP_HASH160 &&
            (*this)[1] == 0x14 && (*this)[22] == OP_EQUAL);
}

bool CScript::IsPayToPubKeyHash() const {
    // Extra-fast test for P2PKH CScripts:
    return size() == 25 && (*this)[0] == OP_DUP && (*this)[1] == OP_HASH160
            && (*this)[2] == 20 && (*this)[23] == OP_EQUALVERIFY
            && (*this)[24] == OP_CHECKSIG;
}

bool CScript::IsCommitment(const std::vector<uint8_t> &data) const {
    // To ensure we have an immediate push, we limit the commitment size to 64
    // bytes. In addition to the data themselves, we have 2 extra bytes:
    // OP_RETURN and the push opcode itself.
    if (data.size() > 64 || this->size() != data.size() + 2) {
        return false;
    }

    if ((*this)[0] != OP_RETURN || (*this)[1] != data.size()) {
        return false;
    }

    for (size_t i = 0; i < data.size(); i++) {
        if ((*this)[i + 2] != data[i]) {
            return false;
        }
    }

    return true;
}

// A witness program is any valid CScript that consists of a 1-byte push opcode
// followed by a data push between 2 and 40 bytes.
bool CScript::IsWitnessProgram(int &version,
                               std::vector<uint8_t> &program) const {
    if (this->size() < 4 || this->size() > 42) {
        return false;
    }
    if ((*this)[0] != OP_0 && ((*this)[0] < OP_1 || (*this)[0] > OP_16)) {
        return false;
    }
    if (size_t((*this)[1] + 2) == this->size()) {
        version = DecodeOP_N((opcodetype)(*this)[0]);
        program = std::vector<uint8_t>(this->begin() + 2, this->end());
        return true;
    }
    return false;
}

// Wrapper returning only the predicate
bool CScript::IsWitnessProgram() const {
    int version;
    std::vector<uint8_t> program;
    return IsWitnessProgram(version, program);
}

bool CScript::IsPushOnly(const_iterator pc) const {
    while (pc < end()) {
        opcodetype opcode;
        if (!GetOp(pc, opcode)) {
            return false;
        }

        // Note that IsPushOnly() *does* consider OP_RESERVED to be a push-type
        // opcode, however execution of OP_RESERVED fails, so it's not relevant
        // to P2SH/BIP62 as the scriptSig would fail prior to the P2SH special
        // validation code being executed.
        if (opcode > OP_16) {
            return false;
        }
    }
    return true;
}

bool CScript::GetPushRefs(
    const_iterator pc, 
    std::set<uint288> &pushRefSet,
    std::set<uint288> &requireRefSet,
    std::set<uint288> &disallowedSiblingsRefSet,
    std::set<uint288> &singletonRefSet,
    uint32_t &stateSeperatorByteIndex
) const {

    std::set<uint288> foundDisallowedRefs;
    std::set<uint288> foundDisallowedSiblingRefs;
    std::set<uint288> foundPushRefs;
    std::set<uint288> foundRequiredRefs;
    std::set<uint288> foundSingletonRefs;

    bool isExistingStateSeperator = false;
    bool isExistingOpReturn = false;
    // Track if there is an OP_STATESEPARATOR
    const_iterator startIterator = pc;
    const_iterator stateSeperatorLocatedIt = pc;
    while (pc < end()) {
        opcodetype opcode;
        std::vector<uint8_t> ret;
        if (!GetOp(pc, opcode, ret)) {
            return false;
        }
        // Track if there is an OP_RETURN
        // If there is one, then cannot use OP_STATESEPARATOR in same script
        if (opcode == OP_RETURN) {
            isExistingOpReturn = true;
        }
        if (opcode == OP_PUSHINPUTREF ||
            opcode == OP_REQUIREINPUTREF ||
            opcode == OP_DISALLOWPUSHINPUTREF ||
            opcode == OP_DISALLOWPUSHINPUTREFSIBLING || 
            opcode == OP_PUSHINPUTREFSINGLETON) {
            std::vector<unsigned char> refId(ret.cbegin(), ret.cend());
            uint288 refIdUint288 = uint288(refId);
            if (opcode == OP_PUSHINPUTREF) {
                foundPushRefs.insert(refIdUint288);
            } else if (opcode == OP_REQUIREINPUTREF) {
                foundRequiredRefs.insert(refIdUint288);
            } else if (opcode == OP_DISALLOWPUSHINPUTREF) {
                foundDisallowedRefs.insert(refIdUint288);
            } else if (opcode == OP_DISALLOWPUSHINPUTREFSIBLING) {
                foundDisallowedSiblingRefs.insert(refIdUint288);
            } else if (opcode == OP_PUSHINPUTREFSINGLETON) {
                // The singleton op code ensures the reference is passed, and simultaneously disallows other siblings 
                // from taking on that reference
                foundPushRefs.insert(refIdUint288);
                foundDisallowedSiblingRefs.insert(refIdUint288);
                foundSingletonRefs.insert(refIdUint288);
            } 
        }
        // Cannot process if there is already an existing state seperator
        // Maximum one OP_STATESEPARATOR allowed per script
        if (isExistingStateSeperator) {
            if (opcode == OP_STATESEPARATOR) {
               return false;
            }
        } else if (!isExistingStateSeperator) {
            if (opcode == OP_STATESEPARATOR) {
                if (isExistingOpReturn) {
                    // OP_STATESEPARATOR cannot be used with OP_RETURN
                    return false;
                }
                stateSeperatorLocatedIt = pc;
                isExistingStateSeperator = true;
            }
        } 
    }
    // Verify that none of the prohibit refs appear in the ref set
    // Save the set difference into resultSet
    std::set<uint288> setOfDisallowedRefs;
    std::insert_iterator< std::set<uint288> > insertIter(setOfDisallowedRefs, setOfDisallowedRefs.begin());
    std::set_intersection(foundDisallowedRefs.begin(), foundDisallowedRefs.end(), foundPushRefs.begin(), foundPushRefs.end(), insertIter);
    // The rule is fulfilled if there are no prohibited references appearing anywhere
    // There should be none of the prohibit refs
    if (setOfDisallowedRefs.size() > 0) {
        return false;
    }
    // Merge in the found refs now that they are okay
    pushRefSet.insert(foundPushRefs.begin(), foundPushRefs.end());
    requireRefSet.insert(foundRequiredRefs.begin(), foundRequiredRefs.end());
    disallowedSiblingsRefSet.insert(foundDisallowedSiblingRefs.begin(), foundDisallowedSiblingRefs.end());
    singletonRefSet.insert(foundSingletonRefs.begin(), foundSingletonRefs.end());
    // Update the location of the state seperator only if we got this far
    // If there was no OP_STATESEPARATOR, then the index is 0 (ie the start of the script)
    // If there was one OP_STATESEPARATOR, then the index is the location of it in the script
    stateSeperatorByteIndex = isExistingStateSeperator ? std::distance(startIterator, stateSeperatorLocatedIt) : 0;
    return true;
}

bool CScript::GetPushRefs(
    std::set<uint288> &pushRefSet,
    std::set<uint288> &requireRefSet,
    std::set<uint288> &disallowedSiblingsRefSet,
    std::set<uint288> &singletonRefSet,
    uint32_t &stateSeperatorByteIndex
) const {
    return this->GetPushRefs(begin(), pushRefSet, requireRefSet, disallowedSiblingsRefSet, singletonRefSet, stateSeperatorByteIndex);
}

static bool getScriptIterator(int32_t startIndex, const CScript &script, CScriptBase::const_iterator &startIterator) {
    uint32_t adjustedStartIndex = startIndex;
    if (startIndex <= 0) {
       adjustedStartIndex = 0;
    }
    if (adjustedStartIndex >= script.size()) {
        return false;
    }
   startIterator = script.begin() + adjustedStartIndex;
   return true;
}

bool CScript::IsPushOnly() const {
    return this->IsPushOnly(begin());
}

bool GetScriptOp(CScriptBase::const_iterator &pc,
                 CScriptBase::const_iterator end, opcodetype &opcodeRet,
                 std::vector<uint8_t> *pvchRet) {
    opcodeRet = INVALIDOPCODE;
    if (pvchRet) {
        pvchRet->clear();
    }
    if (pc >= end) {
        return false;
    }

    // Read instruction
    if (end - pc < 1) {
        return false;
    }

    uint32_t opcode = *pc++;

    // Immediate operand
    if (opcode <= OP_PUSHDATA4) {
        uint32_t nSize = 0;
        if (opcode < OP_PUSHDATA1) {
            nSize = opcode;
        } else if (opcode == OP_PUSHDATA1) {
            if (end - pc < 1) {
                return false;
            }
            nSize = *pc++;
        } else if (opcode == OP_PUSHDATA2) {
            if (end - pc < 2) {
                return false;
            }
            nSize = ReadLE16(&pc[0]);
            pc += 2;
        } else if (opcode == OP_PUSHDATA4) {
            if (end - pc < 4) {
                return false;
            }
            nSize = ReadLE32(&pc[0]);
            pc += 4;
        }
        if (end - pc < 0 || uint32_t(end - pc) < nSize) {
            return false;
        }
        if (pvchRet) {
            pvchRet->assign(pc, pc + nSize);
        }
        pc += nSize;
    } else if (
        opcode == OP_PUSHINPUTREF || 
        opcode == OP_REQUIREINPUTREF || 
        opcode == OP_DISALLOWPUSHINPUTREF || 
        opcode == OP_DISALLOWPUSHINPUTREFSIBLING ||
        opcode == OP_PUSHINPUTREFSINGLETON
    ) {
        // The program counter is incremented for the payload 36 byte arg
        if (end - pc < 36) {
            return false;
        }
        if (end - pc < 0 || uint32_t(end - pc) < 36) {
            return false;
        }
        if (pvchRet) {
            pvchRet->assign(pc, pc + 36);
        }
        pc += 36;
    }
    opcodeRet = static_cast<opcodetype>(opcode);
    return true;
}

bool CScript::HasValidOps() const {
    CScript::const_iterator it = begin();
    while (it < end()) {
        opcodetype opcode;
        std::vector<uint8_t> item;
        if (!GetOp(it, opcode, item) || opcode > MAX_OPCODE ||
            item.size() > MAX_SCRIPT_ELEMENT_SIZE) {
            return false;
        }
    }
    return true;
}
