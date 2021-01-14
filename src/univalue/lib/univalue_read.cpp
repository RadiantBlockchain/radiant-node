// Copyright 2014 BitPay Inc.
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <utility>
#include <vector>
#include "univalue.h"
#include "univalue_utffilter.h"

namespace {
/*
 * According to stackexchange, the original json test suite wanted
 * to limit depth to 22.  Widely-deployed PHP bails at depth 512,
 * so we will follow PHP's lead, which should be more than sufficient
 * (further stackexchange comments indicate depth > 32 rarely occurs).
 */
constexpr size_t MAX_JSON_DEPTH = 512;

constexpr bool json_isdigit(char ch) noexcept
{
    return ch >= '0' && ch <= '9';
}

/// Helper for getJsonToken; converts hexadecimal string to unsigned integer.
///
/// Returns a nullopt if conversion fails due to not enough characters (requires
/// minimum 4 characters), or if any of the characters encountered are not hex.
///
/// On success, consumes the first 4 characters in `buffer`, and returns an
/// optional containing the converted codepoint.
std::optional<unsigned> hatoui(std::string_view& buffer) noexcept
{
    if (buffer.size() < 4)
        return std::nullopt; // not enough digits, fail

    unsigned val = 0;
    for (const char c : buffer.substr(0, 4)) {
        int digit;
        if (json_isdigit(c))
            digit = c - '0';

        else if (c >= 'a' && c <= 'f')
            digit = c - 'a' + 10;

        else if (c >= 'A' && c <= 'F')
            digit = c - 'A' + 10;

        else
            return std::nullopt; // not a hex digit, fail

        val = 16 * val + digit;
    }

    buffer.remove_prefix(4); // consume hex chars from start of buffer
    return val;
}
} // end anonymous namespace

jtokentype getJsonToken(std::string& tokenVal, std::string_view& buffer)
{
    using namespace std::literals; // for operator""sv (string_view literals)

    tokenVal.clear();

    while (!buffer.empty() && json_isspace(buffer.front()))          // skip whitespace
        buffer.remove_prefix(1);

    if (buffer.empty())
        return JTOK_NONE;

    switch (buffer.front()) {

    case '{':
        buffer.remove_prefix(1);
        return JTOK_OBJ_OPEN;
    case '}':
        buffer.remove_prefix(1);
        return JTOK_OBJ_CLOSE;
    case '[':
        buffer.remove_prefix(1);
        return JTOK_ARR_OPEN;
    case ']':
        buffer.remove_prefix(1);
        return JTOK_ARR_CLOSE;

    case ':':
        buffer.remove_prefix(1);
        return JTOK_COLON;
    case ',':
        buffer.remove_prefix(1);
        return JTOK_COMMA;

    case 'n':
        // equivalent to C++20: buffer.starts_with("null")
        if (const auto& tok = "null"sv; tok == buffer.substr(0, tok.size())) {
            buffer.remove_prefix(tok.size());
            return JTOK_KW_NULL;
        }
        return JTOK_ERR;
    case 't':
        // equivalent to C++20: buffer.starts_with("true")
        if (const auto& tok = "true"sv; tok == buffer.substr(0, tok.size())) {
            buffer.remove_prefix(tok.size());
            return JTOK_KW_TRUE;
        }
        return JTOK_ERR;
    case 'f':
        // equivalent to C++20: buffer.starts_with("false")
        if (const auto& tok = "false"sv; tok == buffer.substr(0, tok.size())) {
            buffer.remove_prefix(tok.size());
            return JTOK_KW_FALSE;
        }
        return JTOK_ERR;

    case '-':
    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
    case '8':
    case '9': {
        // part 1: int
        const char * const first = buffer.data();
        const bool firstIsMinus = *first == '-';

        const char * const firstDigit = first + firstIsMinus;

        if (*firstDigit == '0' && firstDigit < &buffer.back() && json_isdigit(firstDigit[1]))
            return JTOK_ERR;

        buffer.remove_prefix(1);                                                       // consume first char

        if (firstIsMinus && (buffer.empty() || !json_isdigit(buffer.front()))) {
            // reject buffers ending in '-' or '-' followed by non-digit
            return JTOK_ERR;
        }

        while (!buffer.empty() && json_isdigit(buffer.front())) {                      // consume digits
            buffer.remove_prefix(1);
        }

        // part 2: frac
        if (!buffer.empty() && buffer.front() == '.') {
            buffer.remove_prefix(1);                                                   // consume .

            if (buffer.empty() || !json_isdigit(buffer.front()))
                return JTOK_ERR;
            do {                                                                       // consume digits
                buffer.remove_prefix(1);
            } while (!buffer.empty() && json_isdigit(buffer.front()));
        }

        // part 3: exp
        if (!buffer.empty() && (buffer.front() == 'e' || buffer.front() == 'E')) {
            buffer.remove_prefix(1);                                                   // consume E

            if (!buffer.empty() && (buffer.front() == '-' || buffer.front() == '+')) { // consume +/-
                buffer.remove_prefix(1);
            }

            if (buffer.empty() || !json_isdigit(buffer.front()))
                return JTOK_ERR;
            do {                                                                       // consume digits
                buffer.remove_prefix(1);
            } while (!buffer.empty() && json_isdigit(buffer.front()));
        }

        tokenVal.assign(first, std::string::size_type(buffer.data() - first));
        return JTOK_NUMBER;
        }

    case '"': {
        buffer.remove_prefix(1);                                // skip "

        std::string valStr;
        JSONUTF8StringFilter writer(valStr);

        while (true) {
            if (buffer.empty() || static_cast<unsigned char>(buffer.front()) < 0x20)
                return JTOK_ERR;

            else if (buffer.front() == '\\') {
                buffer.remove_prefix(1);                        // skip backslash

                if (buffer.empty())
                    return JTOK_ERR;

                const char escChar = buffer.front();
                buffer.remove_prefix(1);                        // skip esc'd char

                switch (escChar) {
                case '"':  writer.push_back('\"'); break;
                case '\\': writer.push_back('\\'); break;
                case '/':  writer.push_back('/'); break;
                case 'b':  writer.push_back('\b'); break;
                case 'f':  writer.push_back('\f'); break;
                case 'n':  writer.push_back('\n'); break;
                case 'r':  writer.push_back('\r'); break;
                case 't':  writer.push_back('\t'); break;

                case 'u':
                    if (auto optCodepoint = hatoui(buffer)) {
                        writer.push_back_u(*optCodepoint);
                        break;
                    }
                    return JTOK_ERR;
                default:
                    return JTOK_ERR;

                }
            }

            else if (buffer.front() == '"') {
                buffer.remove_prefix(1);                        // skip "
                break;                                          // stop scanning
            }

            else {
                writer.push_back(buffer.front());
                buffer.remove_prefix(1);
            }
        }

        if (!writer.finalize())
            return JTOK_ERR;
        tokenVal = std::move(valStr);
        return JTOK_STRING;
        }

    default:
        return JTOK_ERR;
    }
}

enum expect_bits {
    EXP_OBJ_NAME = (1U << 0),
    EXP_COLON = (1U << 1),
    EXP_ARR_VALUE = (1U << 2),
    EXP_VALUE = (1U << 3),
    EXP_NOT_VALUE = (1U << 4),
};

#define expect(bit) (expectMask & (EXP_##bit))
#define setExpect(bit) (expectMask |= EXP_##bit)
#define clearExpect(bit) (expectMask &= ~EXP_##bit)

bool UniValue::read(std::string_view buffer)
{
    setNull();

    uint32_t expectMask = 0;
    std::vector<UniValue*> stack;

    std::string tokenVal;
    jtokentype tok = JTOK_NONE;
    jtokentype last_tok = JTOK_NONE;
    do {
        last_tok = tok;

        tok = getJsonToken(tokenVal, buffer);
        if (tok == JTOK_NONE || tok == JTOK_ERR)
            return false;

        bool isValueOpen = jsonTokenIsValue(tok) ||
            tok == JTOK_OBJ_OPEN || tok == JTOK_ARR_OPEN;

        if (expect(VALUE)) {
            if (!isValueOpen)
                return false;
            clearExpect(VALUE);

        } else if (expect(ARR_VALUE)) {
            bool isArrValue = isValueOpen || (tok == JTOK_ARR_CLOSE);
            if (!isArrValue)
                return false;

            clearExpect(ARR_VALUE);

        } else if (expect(OBJ_NAME)) {
            bool isObjName = (tok == JTOK_OBJ_CLOSE || tok == JTOK_STRING);
            if (!isObjName)
                return false;

        } else if (expect(COLON)) {
            if (tok != JTOK_COLON)
                return false;
            clearExpect(COLON);

        } else if (!expect(COLON) && (tok == JTOK_COLON)) {
            return false;
        }

        if (expect(NOT_VALUE)) {
            if (isValueOpen)
                return false;
            clearExpect(NOT_VALUE);
        }

        switch (tok) {

        case JTOK_OBJ_OPEN:
        case JTOK_ARR_OPEN: {
            VType utyp = (tok == JTOK_OBJ_OPEN ? VOBJ : VARR);
            if (!stack.size()) {
                if (utyp == VOBJ)
                    setObject();
                else
                    setArray();
                stack.push_back(this);
            } else {
                UniValue *top = stack.back();
                if (top->typ == VOBJ) {
                    auto& value = top->entries.rbegin()->second;
                    if (utyp == VOBJ)
                        value.setObject();
                    else
                        value.setArray();
                    stack.push_back(&value);
                } else {
                    top->values.emplace_back(utyp);
                    stack.push_back(&*top->values.rbegin());
                }
            }

            if (stack.size() > MAX_JSON_DEPTH)
                return false;

            if (utyp == VOBJ)
                setExpect(OBJ_NAME);
            else
                setExpect(ARR_VALUE);
            break;
            }

        case JTOK_OBJ_CLOSE:
        case JTOK_ARR_CLOSE: {
            if (!stack.size() || (last_tok == JTOK_COMMA))
                return false;

            VType utyp = (tok == JTOK_OBJ_CLOSE ? VOBJ : VARR);
            UniValue *top = stack.back();
            if (utyp != top->getType())
                return false;

            stack.pop_back();
            clearExpect(OBJ_NAME);
            setExpect(NOT_VALUE);
            break;
            }

        case JTOK_COLON: {
            if (!stack.size())
                return false;

            UniValue *top = stack.back();
            if (top->getType() != VOBJ)
                return false;

            setExpect(VALUE);
            break;
            }

        case JTOK_COMMA: {
            if (!stack.size() ||
                (last_tok == JTOK_COMMA) || (last_tok == JTOK_ARR_OPEN))
                return false;

            UniValue *top = stack.back();
            if (top->getType() == VOBJ)
                setExpect(OBJ_NAME);
            else
                setExpect(ARR_VALUE);
            break;
            }

        case JTOK_KW_NULL:
        case JTOK_KW_TRUE:
        case JTOK_KW_FALSE: {
            UniValue tmpVal;
            switch (tok) {
            case JTOK_KW_NULL:
                // do nothing more
                break;
            case JTOK_KW_TRUE:
                tmpVal = true;
                break;
            case JTOK_KW_FALSE:
                tmpVal = false;
                break;
            default: /* impossible */ break;
            }

            if (!stack.size()) {
                *this = std::move(tmpVal);
                break;
            }

            UniValue *top = stack.back();
            if (top->typ == VOBJ) {
                top->entries.rbegin()->second = std::move(tmpVal);
            } else {
                top->values.emplace_back(std::move(tmpVal));
            }

            setExpect(NOT_VALUE);
            break;
            }

        case JTOK_NUMBER: {
            UniValue tmpVal(VNUM, std::move(tokenVal));
            if (!stack.size()) {
                *this = std::move(tmpVal);
                break;
            }

            UniValue *top = stack.back();
            if (top->typ == VOBJ) {
                top->entries.rbegin()->second = std::move(tmpVal);
            } else {
                top->values.emplace_back(std::move(tmpVal));
            }

            setExpect(NOT_VALUE);
            break;
            }

        case JTOK_STRING: {
            if (expect(OBJ_NAME)) {
                UniValue *top = stack.back();
                top->entries.emplace_back(std::piecewise_construct,
                                          std::forward_as_tuple(std::move(tokenVal)),
                                          std::forward_as_tuple());
                clearExpect(OBJ_NAME);
                setExpect(COLON);
            } else {
                UniValue tmpVal(VSTR, std::move(tokenVal));
                if (!stack.size()) {
                    *this = std::move(tmpVal);
                    break;
                }
                UniValue *top = stack.back();
                if (top->typ == VOBJ) {
                    top->entries.rbegin()->second = std::move(tmpVal);
                } else {
                    top->values.emplace_back(std::move(tmpVal));
                }
            }

            setExpect(NOT_VALUE);
            break;
            }

        default:
            return false;
        }
    } while (!stack.empty ());

    /* Check that nothing follows the initial construct (parsed above).  */
    tok = getJsonToken(tokenVal, buffer);
    if (tok != JTOK_NONE)
        return false;

    return true;
}
