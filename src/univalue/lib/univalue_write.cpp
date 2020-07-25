// Copyright 2014 BitPay Inc.
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <cstring>
#include <stdio.h>
#include "univalue.h"
#include "univalue_escapes.h"

// Opaque type used for writing. This can be further optimized later.
struct UniValue::Stream {
    std::string & str; // this is a reference for RVO to always work in UniValue::write() below
    void put(char c) { str.push_back(c); }
    void put(char c, size_t nFill) { str.append(nFill, c); }
    void write(const char *s, size_t len) { str.append(s, len); }
    Stream & operator<<(const char *s) { str.append(s); return *this; }
    Stream & operator<<(const std::string &s) { str.append(s); return *this; }
};

/* static */
void UniValue::jsonEscape(Stream & ss, const std::string & inS)
{
    for (const auto ch : inS) {
        const char * const escStr = escapes[uint8_t(ch)];

        if (escStr)
            ss << escStr;
        else
            ss.put(ch);
    }
}

std::string UniValue::write(const unsigned int prettyIndent) const
{
    std::string s; // we do it this way for RVO to work on all compilers
    Stream ss{s};
    s.reserve(1024);
    writeStream(ss, prettyIndent, 0);
    return s;
}

void UniValue::writeStream(Stream & ss, const unsigned int prettyIndent, const unsigned int indentLevel) const
{
    switch (typ) {
    case VNULL:
        ss.write("null", 4); // .write() is slightly faster than operator<<
        break;
    case VOBJ:
        writeObject(ss, prettyIndent, indentLevel);
        break;
    case VARR:
        writeArray(ss, prettyIndent, indentLevel);
        break;
    case VSTR:
        ss.put('"');
        jsonEscape(ss, val);
        ss.put('"');
        break;
    case VNUM:
        ss << val;
        break;
    case VBOOL:
        if (val == boolTrueVal)
            ss.write("true", 4);
        else
            ss.write("false", 5);
        break;
    }
}

/* static */
inline void UniValue::startNewLine(Stream & ss, const unsigned int prettyIndent, const unsigned int indentLevel)
{
    if (prettyIndent) {
        ss.put('\n');
        ss.put(' ', indentLevel);
    }
}

void UniValue::writeArray(Stream & ss, const unsigned int prettyIndent, const unsigned int indentLevel) const
{
    ss.put('[');
    if (!values.empty()) {
        const unsigned int internalIndentLevel = indentLevel + prettyIndent;
        for (auto value = values.begin(), end = values.end();;) {
            startNewLine(ss, prettyIndent, internalIndentLevel);
            value->writeStream(ss, prettyIndent, internalIndentLevel);
            if (++value == end) {
                break;
            }
            ss.put(',');
        }
    }
    startNewLine(ss, prettyIndent, indentLevel);
    ss.put(']');
}

void UniValue::writeObject(Stream & ss, const unsigned int prettyIndent, const unsigned int indentLevel) const
{
    ss.put('{');
    if (!entries.empty()) {
        const unsigned int internalIndentLevel = indentLevel + prettyIndent;
        for (auto entry = entries.begin(), end = entries.end();;) {
            startNewLine(ss, prettyIndent, internalIndentLevel);
            ss.put('"');
            jsonEscape(ss, entry->first);
            ss.write("\":", 2);
            if (prettyIndent) {
                ss.put(' ');
            }
            entry->second.writeStream(ss, prettyIndent, internalIndentLevel);
            if (++entry == end) {
                break;
            }
            ss.put(',');
        }
    }
    startNewLine(ss, prettyIndent, indentLevel);
    ss.put('}');
}
