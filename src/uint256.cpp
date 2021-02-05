// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <uint256.h>

#include <util/strencodings.h>

#include <cstring>
#include <iterator>
#include <type_traits>

// Some basic compile-time checks
static_assert(uint256{}.IsNull() && uint160{}.IsNull(),
              "Default constructed base_blob should be IsNull()");
static_assert(std::is_pointer_v<decltype(uint256{}.begin())>,
              "The below code assumes begin() is a simple pointer to uint8_t");

template <unsigned int BITS>
base_blob<BITS>::base_blob(const std::vector<uint8_t> &vch) noexcept {
    assert(vch.size() == size());
    std::memcpy(begin(), vch.data(), size());
}

template <unsigned int BITS> std::string base_blob<BITS>::GetHex() const {
    return HexStr(std::reverse_iterator(end()), std::reverse_iterator(begin()));
}

template <unsigned int BITS> void base_blob<BITS>::SetHex(const char *psz) noexcept {

    // skip leading spaces
    while (IsSpace(*psz)) {
        psz++;
    }

    // skip 0x
    if (psz[0] == '0' && ToLower(uint8_t(psz[1])) == 'x') {
        psz += 2;
    }

    // hex string to uint
    const char *const pbegin = psz;
    while (::HexDigit(*psz) != -1) {
        psz++;
    }

    psz--;
    uint8_t *p1 = begin();
    const uint8_t *const pend = end();
    while (psz >= pbegin && p1 < pend) {
        *p1 = ::HexDigit(*psz--);
        if (psz >= pbegin) {
            *p1 |= uint8_t(::HexDigit(*psz--) << 4);
        }
        ++p1;
    }

    // clear remaining bytes, if any
    while (p1 < pend)
        *p1++ = 0;
}

template <unsigned int BITS>
void base_blob<BITS>::SetHex(const std::string &str) noexcept {
    SetHex(str.c_str());
}

// Explicit instantiations for base_blob<160>
template base_blob<160>::base_blob(const std::vector<uint8_t> &) noexcept;
template std::string base_blob<160>::GetHex() const;
template std::string base_blob<160>::ToString() const;
template void base_blob<160>::SetHex(const char *) noexcept;
template void base_blob<160>::SetHex(const std::string &) noexcept;

// Explicit instantiations for base_blob<256>
template base_blob<256>::base_blob(const std::vector<uint8_t> &) noexcept;
template std::string base_blob<256>::GetHex() const;
template std::string base_blob<256>::ToString() const;
template void base_blob<256>::SetHex(const char *) noexcept;
template void base_blob<256>::SetHex(const std::string &) noexcept;
