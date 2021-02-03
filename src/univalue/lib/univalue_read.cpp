// Copyright 2014 BitPay Inc.
// Copyright (c) 2020-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <optional>

#include "univalue.h"
#include "univalue_utffilter.h"

namespace {

/**
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

// Attention: in several functions below, you are reading a NUL-terminated string.
// Always assure yourself that a character is not NUL, prior to advancing the pointer to the next character.

/**
 * Helper for getJsonToken; converts hexadecimal string to unsigned integer.
 *
 * Returns a nullopt if conversion fails due to not enough characters (requires
 * minimum 4 characters), or if any of the characters encountered are not hex.
 *
 * On success, returns an optional containing the converted codepoint.
 *
 * Consumes the first 4 hex characters in `buffer`, insofar they exist.
 */
constexpr std::optional<unsigned> hatoui(const char*& buffer) noexcept
{
    unsigned val = 0;
    for (const char* end = buffer + 4; buffer != end; ++buffer) {  // consume 4 chars from buffer
        val *= 16;
        if (json_isdigit(*buffer))
            val += *buffer - '0';
        else if (*buffer >= 'a' && *buffer <= 'f')
            val += *buffer - 'a' + 10;
        else if (*buffer >= 'A' && *buffer <= 'F')
            val += *buffer - 'A' + 10;
        else
            return std::nullopt; // not a hex digit, fail
    }
    return val;
}

} // end anonymous namespace

jtokentype getJsonToken(std::string& tokenVal, const char*& buffer)
{
    tokenVal.clear();

    while (json_isspace(*buffer))          // skip whitespace
        ++buffer;

    switch (*buffer) {

    case '\0': // terminating NUL
        return JTOK_NONE;

    case '{':
        ++buffer;
        return JTOK_OBJ_OPEN;
    case '}':
        ++buffer;
        return JTOK_OBJ_CLOSE;
    case '[':
        ++buffer;
        return JTOK_ARR_OPEN;
    case ']':
        ++buffer;
        return JTOK_ARR_CLOSE;

    case ':':
        ++buffer;
        return JTOK_COLON;
    case ',':
        ++buffer;
        return JTOK_COMMA;

    case 'n': // null
        if (*++buffer == 'u' && *++buffer == 'l' && *++buffer == 'l') {
            ++buffer;
            return JTOK_KW_NULL;
        }
        return JTOK_ERR;
    case 't': // true
        if (*++buffer == 'r' && *++buffer == 'u' && *++buffer == 'e') {
            ++buffer;
            return JTOK_KW_TRUE;
        }
        return JTOK_ERR;
    case 'f': // false
        if (*++buffer == 'a' && *++buffer == 'l' && *++buffer == 's' && *++buffer == 'e') {
            ++buffer;
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
        const char * const first = buffer;
        const bool firstIsMinus = *first == '-';

        const char * const firstDigit = first + firstIsMinus;

        if (*firstDigit == '0' && json_isdigit(firstDigit[1]))
            return JTOK_ERR;

        ++buffer;                                                       // consume first char

        if (firstIsMinus && !json_isdigit(*buffer)) {
            // reject buffers ending in '-' or '-' followed by non-digit
            return JTOK_ERR;
        }

        while (json_isdigit(*buffer)) {                      // consume digits
            ++buffer;
        }

        // part 2: frac
        if (*buffer == '.') {
            ++buffer;                                                   // consume .

            if (!json_isdigit(*buffer))
                return JTOK_ERR;
            do {                                                                       // consume digits
                ++buffer;
            } while (json_isdigit(*buffer));
        }

        // part 3: exp
        if (*buffer == 'e' || *buffer == 'E') {
            ++buffer;                                                   // consume E

            if (*buffer == '-' || *buffer == '+') { // consume +/-
                ++buffer;
            }

            if (!json_isdigit(*buffer))
                return JTOK_ERR;
            do {                                                                       // consume digits
                ++buffer;
            } while (json_isdigit(*buffer));
        }

        tokenVal.assign(first, buffer);
        return JTOK_NUMBER;
        }

    case '"': {
        ++buffer;                                // skip "

        std::string valStr;
        JSONUTF8StringFilter writer(valStr);

        for (;;) {
            if (static_cast<unsigned char>(*buffer) < 0x20) {
                return JTOK_ERR;
            } else if (*buffer == '\\') {
                switch (*++buffer) {  // skip backslash
                case '"':  writer.push_back('\"'); ++buffer; break;
                case '\\': writer.push_back('\\'); ++buffer; break;
                case '/':  writer.push_back('/');  ++buffer; break;
                case 'b':  writer.push_back('\b'); ++buffer; break;
                case 'f':  writer.push_back('\f'); ++buffer; break;
                case 'n':  writer.push_back('\n'); ++buffer; break;
                case 'r':  writer.push_back('\r'); ++buffer; break;
                case 't':  writer.push_back('\t'); ++buffer; break;
                case 'u':
                    if (auto optCodepoint = hatoui(++buffer)) { // skip u
                        writer.push_back_u(*optCodepoint);
                        break;
                    }
                    [[fallthrough]];
                default:
                    return JTOK_ERR;
                }
            } else if (*buffer == '"') {
                ++buffer;                        // skip "
                break;                                          // stop scanning
            } else {
                writer.push_back(*buffer++);
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

const char* UniValue::read(const char* buffer)
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
            return nullptr;

        bool isValueOpen = jsonTokenIsValue(tok) ||
            tok == JTOK_OBJ_OPEN || tok == JTOK_ARR_OPEN;

        if (expect(VALUE)) {
            if (!isValueOpen)
                return nullptr;
            clearExpect(VALUE);

        } else if (expect(ARR_VALUE)) {
            bool isArrValue = isValueOpen || (tok == JTOK_ARR_CLOSE);
            if (!isArrValue)
                return nullptr;

            clearExpect(ARR_VALUE);

        } else if (expect(OBJ_NAME)) {
            bool isObjName = (tok == JTOK_OBJ_CLOSE || tok == JTOK_STRING);
            if (!isObjName)
                return nullptr;

        } else if (expect(COLON)) {
            if (tok != JTOK_COLON)
                return nullptr;
            clearExpect(COLON);

        } else if (!expect(COLON) && (tok == JTOK_COLON)) {
            return nullptr;
        }

        if (expect(NOT_VALUE)) {
            if (isValueOpen)
                return nullptr;
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
                return nullptr;

            if (utyp == VOBJ)
                setExpect(OBJ_NAME);
            else
                setExpect(ARR_VALUE);
            break;
            }

        case JTOK_OBJ_CLOSE:
        case JTOK_ARR_CLOSE: {
            if (!stack.size() || (last_tok == JTOK_COMMA))
                return nullptr;

            VType utyp = (tok == JTOK_OBJ_CLOSE ? VOBJ : VARR);
            UniValue *top = stack.back();
            if (utyp != top->getType())
                return nullptr;

            stack.pop_back();
            clearExpect(OBJ_NAME);
            setExpect(NOT_VALUE);
            break;
            }

        case JTOK_COLON: {
            if (!stack.size())
                return nullptr;

            UniValue *top = stack.back();
            if (top->getType() != VOBJ)
                return nullptr;

            setExpect(VALUE);
            break;
            }

        case JTOK_COMMA: {
            if (!stack.size() ||
                (last_tok == JTOK_COMMA) || (last_tok == JTOK_ARR_OPEN))
                return nullptr;

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
            return nullptr;
        }
    } while (!stack.empty());

    /* Check that nothing follows the initial construct (parsed above).  */
    tok = getJsonToken(tokenVal, buffer);
    if (tok != JTOK_NONE)
        return nullptr;

    return buffer;
}

bool UniValue::read(const std::string& raw)
{
    // JSON containing unescaped NUL characters is invalid.
    // std::string is NUL-terminated but may also contain NULs within its size.
    // So read until the first NUL character, and then verify that this is indeed the terminating NUL.
    return read(raw.data()) == raw.data() + raw.size();
}
