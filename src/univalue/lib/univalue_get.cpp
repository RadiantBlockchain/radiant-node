// Copyright 2014 BitPay Inc.
// Copyright 2015 Bitcoin Core Developers
// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or https://opensource.org/licenses/mit-license.php.

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdexcept>
#include <vector>
#include <limits>
#include <string>
#include <sstream>        // .get_int64()

#include "univalue.h"

namespace
{
static bool ParsePrechecks(const std::string& str) noexcept
{
    if (str.empty()) // No empty string allowed
        return false;
    if (json_isspace(str[0]) || json_isspace(str[str.size()-1])) // No padding allowed
        return false;
    if (str.size() != strlen(str.c_str())) // No embedded NUL characters allowed
        return false;
    return true;
}

bool ParseInt32(const std::string& str, int32_t *out) noexcept
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtol will not set errno if valid
    long int n = strtol(str.c_str(), &endp, 10);
    if(out) *out = (int32_t)n;
    // Note that strtol returns a *long int*, so even if strtol doesn't report an over/underflow
    // we still have to check that the returned value is within the range of an *int32_t*. On 64-bit
    // platforms the size of these types may be different.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int32_t>::min() &&
        n <= std::numeric_limits<int32_t>::max();
}

bool ParseInt64(const std::string& str, int64_t *out) noexcept
{
    if (!ParsePrechecks(str))
        return false;
    char *endp = NULL;
    errno = 0; // strtoll will not set errno if valid
    long long int n = strtoll(str.c_str(), &endp, 10);
    if(out) *out = (int64_t)n;
    // Note that strtoll returns a *long long int*, so even if strtol doesn't report a over/underflow
    // we still have to check that the returned value is within the range of an *int64_t*.
    return endp && *endp == 0 && !errno &&
        n >= std::numeric_limits<int64_t>::min() &&
        n <= std::numeric_limits<int64_t>::max();
}

bool ParseDouble(const std::string& str, double *out)
{
    if (!ParsePrechecks(str))
        return false;
    if (str.size() >= 2 && str[0] == '0' && str[1] == 'x') // No hexadecimal floats allowed
        return false;
    std::istringstream text(str);
    text.imbue(std::locale::classic());
    double result;
    text >> result;
    if(out) *out = result;
    return text.eof() && !text.fail();
}
} // end anonymous namespace

bool UniValue::get_bool() const
{
    if (!isBool())
        throw std::runtime_error("JSON value is not a boolean as expected");
    return getBool();
}

int UniValue::get_int() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not an integer as expected");
    int32_t retval;
    if (!ParseInt32(getValStr(), &retval))
        throw std::runtime_error("JSON integer out of range");
    return retval;
}

int64_t UniValue::get_int64() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not an integer as expected");
    int64_t retval;
    if (!ParseInt64(getValStr(), &retval))
        throw std::runtime_error("JSON integer out of range");
    return retval;
}

double UniValue::get_real() const
{
    if (!isNum())
        throw std::runtime_error("JSON value is not a number as expected");
    double retval;
    if (!ParseDouble(getValStr(), &retval))
        throw std::runtime_error("JSON double out of range");
    return retval;
}

const std::string& UniValue::get_str() const
{
    if (!isStr())
        throw std::runtime_error("JSON value is not a string as expected");
    return val;
}
std::string& UniValue::get_str()
{
    if (!isStr())
        throw std::runtime_error("JSON value is not a string as expected");
    return val;
}

const UniValue::Object& UniValue::get_obj() const
{
    if (!isObject())
        throw std::runtime_error("JSON value is not an object as expected");
    return entries;
}
UniValue::Object& UniValue::get_obj()
{
    if (!isObject())
        throw std::runtime_error("JSON value is not an object as expected");
    return entries;
}

const UniValue::Array& UniValue::get_array() const
{
    if (!isArray())
        throw std::runtime_error("JSON value is not an array as expected");
    return values;
}
UniValue::Array& UniValue::get_array()
{
    if (!isArray())
        throw std::runtime_error("JSON value is not an array as expected");
    return values;
}