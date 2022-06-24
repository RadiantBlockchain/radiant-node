// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <crypto/common.h>
#include <crypto/ripemd160.h>
#include <crypto/sha256.h>
#include <crypto/sha512_256.h>
#include <crypto/sha512.h>
#include <crypto/siphash.h>
#include <prevector.h>
#include <serialize.h>
#include <uint256.h>
#include <version.h>

#include <vector>

typedef uint256 ChainCode;

/** A hasher class for Bitcoin's 256-bit hash (double SHA-256). */
class CHash256 {
private:
    CSHA256 sha;

public:
    static const size_t OUTPUT_SIZE = CSHA256::OUTPUT_SIZE;

    void Finalize(Span<uint8_t> output) {
        assert(output.size() == OUTPUT_SIZE);
        uint8_t buf[CSHA256::OUTPUT_SIZE];
        sha.Finalize(buf);
        sha.Reset().Write(buf, CSHA256::OUTPUT_SIZE).Finalize(output.data());
    }

    void FinalizeSHA256(Span<uint8_t> output) {
        assert(output.size() == OUTPUT_SIZE);
        sha.Finalize(output.data());
    }

    CHash256 &Write(Span<const uint8_t> input) {
        sha.Write(input.data(), input.size());
        return *this;
    }

    CHash256 &Reset() {
        sha.Reset();
        return *this;
    }
};

/** A hasher class for 512/256-bit hash (double SHA-512/256). */
class CHash512_256 {
private:
    CSHA512_256 sha;

public:
    static const size_t OUTPUT_SIZE = CSHA512_256::OUTPUT_SIZE;

    void Finalize(Span<uint8_t> output) {
        assert(output.size() == OUTPUT_SIZE);
        uint8_t buf[CSHA512_256::OUTPUT_SIZE];
        sha.Finalize(buf);
        sha.Reset().Write(buf, CSHA512_256::OUTPUT_SIZE).Finalize(output.data());
    }

    CHash512_256 &Write(Span<const uint8_t> input) {
        sha.Write(input.data(), input.size());
        return *this;
    }

    CHash512_256 &Reset() {
        sha.Reset();
        return *this;
    }
};

/** A hasher class for Bitcoin's 160-bit hash (SHA-256 + RIPEMD-160). */
class CHash160 {
private:
    CSHA256 sha;

public:
    static const size_t OUTPUT_SIZE = CRIPEMD160::OUTPUT_SIZE;

    void Finalize(Span<uint8_t> output) {
        assert(output.size() == OUTPUT_SIZE);
        uint8_t buf[CSHA256::OUTPUT_SIZE];
        sha.Finalize(buf);
        CRIPEMD160().Write(buf, CSHA256::OUTPUT_SIZE).Finalize(output.data());
    }

    CHash160 &Write(Span<const uint8_t> input) {
        sha.Write(input.data(), input.size());
        return *this;
    }

    CHash160 &Reset() {
        sha.Reset();
        return *this;
    }
};

/** Compute the 256-bit hash of an object. */
template <typename T>
inline uint256 Hash(const T &in1) {
    uint256 result{uint256::Uninitialized};
    CHash256().Write(MakeUInt8Span(in1)).Finalize(result);
    return result;
}

/** Compute the 256-bit hash of the concatenation of two objects. */
template <typename T1, typename T2>
inline uint256 Hash(const T1 &in1, const T2 &in2) {
    uint256 result{uint256::Uninitialized};
    CHash256().Write(MakeUInt8Span(in1)).Write(MakeUInt8Span(in2)).Finalize(result);
    return result;
}

/** Compute the 160-bit hash an object. */
template <typename T1>
inline uint160 Hash160(const T1 &in1) {
    uint160 result{uint160::Uninitialized};
    CHash160().Write(MakeUInt8Span(in1)).Finalize(result);
    return result;
}

/** A writer stream (for serialization) that computes a 256-bit hash. */
class CHashWriter {
private:
    CHash256 ctx;

    const int nType;
    const int nVersion;

public:
    CHashWriter(int nTypeIn, int nVersionIn)
        : nType(nTypeIn), nVersion(nVersionIn) {}

    int GetType() const { return nType; }
    int GetVersion() const { return nVersion; }

    void write(const char *pch, size_t size) {
        ctx.Write({UInt8Cast(pch), size});
    }

    // invalidates the object
    uint256 GetHash() {
        uint256 result{uint256::Uninitialized};
        ctx.Finalize(result);
        return result;
    }

    /**
     * Returns the first 64 bits from the resulting hash.
     */
    inline uint64_t GetCheapHash() {
        uint8_t result[CHash256::OUTPUT_SIZE];
        ctx.Finalize(result);
        return ReadLE64(result);
    }

    template <typename T> CHashWriter &operator<<(const T &obj) {
        // Serialize to this stream
        ::Serialize(*this, obj);
        return (*this);
    }
};
 

/**
 * Reads data from an underlying stream, while hashing the read data.
 */
template <typename Source> class CHashVerifier : public CHashWriter {
private:
    Source *source;

public:
    explicit CHashVerifier(Source *source_)
        : CHashWriter(source_->GetType(), source_->GetVersion()),
          source(source_) {}

    void read(char *pch, size_t nSize) {
        source->read(pch, nSize);
        this->write(pch, nSize);
    }

    void ignore(size_t nSize) {
        char data[1024];
        while (nSize > 0) {
            size_t now = std::min<size_t>(nSize, 1024);
            read(data, now);
            nSize -= now;
        }
    }

    template <typename T> CHashVerifier<Source> &operator>>(T &&obj) {
        // Unserialize from this stream
        ::Unserialize(*this, obj);
        return (*this);
    }
};

/** Compute the 256-bit hash of an object's serialization. */
template <typename T>
uint256 SerializeHash(const T &obj, int nType = SER_GETHASH,
                      int nVersion = PROTOCOL_VERSION) {
    CHashWriter ss(nType, nVersion);
    ss << obj;
    return ss.GetHash();
}

// MurmurHash3: ultra-fast hash suitable for hash tables but not cryptographically secure
uint32_t MurmurHash3(uint32_t nHashSeed,
                     const uint8_t *pDataToHash, size_t nDataLen /* bytes */);
inline uint32_t MurmurHash3(uint32_t nHashSeed, Span<const uint8_t> vDataToHash) {
    return MurmurHash3(nHashSeed, vDataToHash.data(), vDataToHash.size());
}

void BIP32Hash(const ChainCode &chainCode, uint32_t nChild, uint8_t header,
               const uint8_t data[32], uint8_t output[64]);

/// Hash writer for fast SipHash - Used to get a fast hash for any serializable type
class CSipHashWriter
{
    CSipHasher hasher;
    const int nType, nVersion;
public:
    CSipHashWriter(uint64_t k0, uint64_t k1, int nTypeIn, int nVersionIn) noexcept
        : hasher(k0, k1), nType(nTypeIn), nVersion(nVersionIn)
    {}

    int GetType() const { return nType; }
    int GetVersion() const { return nVersion; }

    void write(const char *pch, size_t size) {
        hasher.Write(reinterpret_cast<const uint8_t *>(pch), size);
    }

    template <typename T> CSipHashWriter &operator<<(const T &obj) {
        // Serialize to this stream
        ::Serialize(*this, obj);
        return *this;
    }

    // Invalidates hasher
    uint64_t GetHash() const { return hasher.Finalize(); }
};

/** Compute the Sip hash of an object's serialization. */
template <typename T>
uint64_t SerializeSipHash(const T &obj, uint64_t k0, uint64_t k1,
                          int nType = SER_GETHASH, int nVersion = PROTOCOL_VERSION) {
    CSipHashWriter ss(k0, k1, nType, nVersion);
    ss << obj;
    return ss.GetHash();
}
