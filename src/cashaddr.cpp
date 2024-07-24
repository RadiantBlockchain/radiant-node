#include "cashaddr.h"
#include <openssl/sha.h>
#include <string>
#include <vector>
#include <stdexcept>
#include <cstring>
#include <algorithm>

namespace {

// Network parameters for Bitcoin Cash legacy addresses
const uint8_t ADDRTYPE_P2PKH = 0x00;
const uint8_t ADDRTYPE_P2SH = 0x05;
const std::string BASE58_CHARS = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

// Base58 encoding function
std::string Base58Encode(const std::vector<uint8_t>& v) {
    std::string result;
    size_t leading_zeros = 0;
    while (leading_zeros < v.size() && v[leading_zeros] == 0) {
        ++leading_zeros;
    }

    std::vector<uint8_t> b58(v.size() * 138 / 100 + 1);
    for (size_t i = leading_zeros; i < v.size(); ++i) {
        int carry = v[i];
        for (auto it = b58.rbegin(); it != b58.rend(); ++it) {
            carry += 256 * (*it);
            *it = carry % 58;
            carry /= 58;
        }
    }

    auto it = b58.begin();
    while (it != b58.end() && *it == 0) {
        ++it;
    }

    result.reserve(leading_zeros + (b58.end() - it));
    result.assign(leading_zeros, '1');
    while (it != b58.end()) {
        result += BASE58_CHARS[*(it++)];
    }

    return result;
}

// Base58 decoding function
std::vector<uint8_t> Base58Decode(const std::string& str) {
    std::vector<uint8_t> result(str.size() * 733 / 1000 + 1);
    for (char c : str) {
        auto pos = BASE58_CHARS.find(c);
        if (pos == std::string::npos) {
            throw std::invalid_argument("Invalid Base58 character");
        }
        int carry = pos;
        for (auto it = result.rbegin(); it != result.rend(); ++it) {
            carry += 58 * (*it);
            *it = carry % 256;
            carry /= 256;
        }
    }

    auto it = result.begin();
    while (it != result.end() && *it == 0) {
        ++it;
    }

    return std::vector<uint8_t>(it, result.end());
}

// Double SHA-256 function
std::vector<uint8_t> DoubleSHA256(const std::vector<uint8_t>& data) {
    std::vector<uint8_t> hash1(SHA256_DIGEST_LENGTH);
    std::vector<uint8_t> hash2(SHA256_DIGEST_LENGTH);
    SHA256(data.data(), data.size(), hash1.data());
    SHA256(hash1.data(), hash1.size(), hash2.data());
    return hash2;
}

// Helper function to encode payload with checksum
std::string EncodeBase58Check(const std::vector<uint8_t>& payload) {
    std::vector<uint8_t> data = payload;
    std::vector<uint8_t> checksum = DoubleSHA256(data);
    data.insert(data.end(), checksum.begin(), checksum.begin() + 4);
    return Base58Encode(data);
}

// Helper function to decode payload and verify checksum
std::vector<uint8_t> DecodeBase58Check(const std::string& address) {
    std::vector<uint8_t> data = Base58Decode(address);
    if (data.size() < 4) {
        throw std::invalid_argument("Invalid address");
    }
    std::vector<uint8_t> payload(data.begin(), data.end() - 4);
    std::vector<uint8_t> checksum(data.end() - 4, data.end());
    std::vector<uint8_t> calculated_checksum = DoubleSHA256(payload);
    if (!std::equal(checksum.begin(), checksum.end(), calculated_checksum.begin())) {
        throw std::invalid_argument("Invalid checksum");
    }
    return payload;
}

} // namespace

namespace cashaddr {

// Encode function for legacy address
std::string Encode(const std::string& prefix, const std::vector<uint8_t>& values) {
    if (prefix.empty() || values.empty()) {
        return "";
    }

    // Prefix is not used for legacy encoding, only the values (payload)
    return EncodeBase58Check(values);
}

// Decode function for legacy address
std::pair<std::string, std::vector<uint8_t>> Decode(const std::string& str, const std::string& default_prefix) {
    if (str.empty()) {
        return {"", {}};
    }

    try {
        std::vector<uint8_t> payload = DecodeBase58Check(str);
        return {default_prefix, payload};
    } catch (const std::exception&) {
        return {"", {}};
    }
}

} // namespace cashaddr
