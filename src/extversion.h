// Copyright (C) 2018 The Bitcoin Unlimited developers
// Copyright (C) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <serialize.h>
#include <streams.h>
#include <tinyformat.h>
#include <version.h>

#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

namespace extversion {

/** Default for -use-extversion CLI option (and whether we advertise the service) */
static constexpr bool DEFAULT_ENABLED = false;

//! Keys we know about and care about, everything else is ignored.
//! The 0.1.0 extversion spec uses 64 bit keys
enum class Key : uint64_t {
    Version = 0x0,
};


//! Encapsulates the extversion message data for Key::Version.
//!
//! Note that due to the way the ExtVersion specification works, .Minor() and
//! .Revision() produce undefined behavior if set to values >= 100.
struct VersionTuple
{
    using Tuple = std::tuple<uint32_t, uint8_t, uint8_t>;
    Tuple tuple;

    constexpr VersionTuple() = default;
    constexpr VersionTuple(uint32_t maj, uint8_t min, uint8_t rev) : tuple{maj, min, rev} {}
    constexpr VersionTuple(const Tuple &t) : tuple{t} {}

    constexpr uint32_t Major() const noexcept { return std::get<0>(tuple); }
    constexpr uint8_t  Minor() const noexcept { return std::get<1>(tuple); }
    constexpr uint8_t  Revision() const noexcept { return std::get<2>(tuple); }

    constexpr uint64_t ToU64() const noexcept {
        return uint64_t(10'000U) * Major() + (100U * Minor()) % 10'000U + Revision() % 100U;
    }
    static constexpr VersionTuple FromU64(const uint64_t u) noexcept {
        const uint32_t maj = u / 10'000U;
        const uint8_t  min = (u - (u / 10'000U) * 10'000U) / 100U;
        const uint8_t  rev = u % 100U;
        return {maj, min, rev};
    }

    //! .Minor() and .Revision() cannot be set >= this value
    static constexpr uint32_t MinorRevisionRange() noexcept { return 100; }

    //! Note: We expose these operators in case they may be useful to callers.
    //! We elected not to inherit from std::tuple but rather use composition
    //! since inheriting from std templates is ill-advised.
    constexpr bool operator< (const VersionTuple &o) const noexcept { return tuple <  o.tuple; }
    constexpr bool operator<=(const VersionTuple &o) const noexcept { return tuple <= o.tuple; }
    constexpr bool operator==(const VersionTuple &o) const noexcept { return tuple == o.tuple; }
    constexpr bool operator>=(const VersionTuple &o) const noexcept { return tuple >= o.tuple; }
    constexpr bool operator> (const VersionTuple &o) const noexcept { return tuple >  o.tuple; }
    constexpr bool operator!=(const VersionTuple &o) const noexcept { return tuple != o.tuple; }
};

//! We are using verson 0.1.0 of the ExtVersion implementation
static constexpr VersionTuple version{0, 1, 0};

//! These are here to illustrate limitations of the spec
static_assert (version.Minor() < version.MinorRevisionRange(), "Static version tuple out of range");
static_assert (version.Revision() < version.MinorRevisionRange(), "Static version tuple out of range");


/*!
  Radiant extended version message implementation.

  This version message de-/serializes the same fields as the version
  message format as in the BU BCH implementation as of July 2018.

  For now we only support Key::Version as the only key we understand
  and serialize/deserialize.  All other unknown keys are silently
  ignored.

  A size of 100kB for the serialized map must not be exceeded.
  The size limit is enforced on serialization, as well as from the
  net_processing.cpp file before deserializing the received network
  message.
*/
class Message final
{
    struct Values {
        std::optional<VersionTuple> version; //! Data received/sent for Key::Version
        // We may add more values here as we add support for more keys

        void clear() noexcept { version.reset(); }
    } values;

public:
    Message() = default;

    //! Gets the value for Key::Version. May return an empty optional.
    std::optional<VersionTuple> GetVersion() const { return values.version; }
    //! Sets the value for Key::Version.
    void SetVersion(const VersionTuple &v = extversion::version) { values.version = v; }

    /* Serialization methods */

    //! Serialized Message may not exceed this size in bytes
    static constexpr size_t MaxSize() noexcept { return 100'000; }

    //! The message we receive may not contain more than this many keys, as a DoS defense
    static constexpr size_t MaxNumKeys() noexcept { return 8192; }

    //! Serialize this instance. This enforces the resulting buffer is <= MaxSize().
    template <typename Stream>
    void Serialize(Stream &s) const
    {
        const size_t startSize = s.size();
        // Write "as-if" it were a map
        const uint64_t nItems = bool(values.version);
        // Write map size (always 0 or 1 currently)
        WriteCompactSize(s, nItems);

        // Note: all "map" items are of the form: key_as_compact_u64, value_data_as_vector
        if (values.version) {
            // Write key for Key::Version (=0x0 as a uint64)
            WriteCompactSize(s, static_cast<uint64_t>(Key::Version));
            // Write data -- serialize the data vector as a uint64_t data item
            std::vector<uint8_t> vData;
            const uint64_t versionValue = values.version->ToU64();
            // Write the version tuple data in-place to vData
            CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, vData, 0, COMPACTSIZE(versionValue));
            // Serialize the temporary vData to the output stream
            s << vData;
        }
        // For now this will always be in bounds, but the check is left-in for future code.
        CheckSize(s.size() - startSize);
    }

    //! Unserialize this instance. Ideally we would enforoce MaxSize() is not exceeded here,
    //! however that's tricky to do since the various stream classes don't offer a way to
    //! track the size of the data as we deserialize, and we don't want to call s.size()
    //! here since the Stream may potentially contain other data after this message is
    //! unserialized.  In this codebase, currently MaxSize() is enforced in net_processing.cpp
    //! when we receive the EXTVERSION message, and right before we unserialize it.
    template <typename Stream>
    void Unserialize(Stream &s)
    {
        // Read "as-if" it were a map
        values.clear(); // ensure a clean slate
        const uint64_t nItems = ReadCompactSizeWithLimit(s, MaxNumKeys()); // read number of key/value pairs
        for (uint64_t i = 0; i < nItems; ++i) {
            constexpr auto KeyValueLimit = std::numeric_limits<uint64_t>::max(); // key/values can use full 64-bit range
            const uint64_t key = ReadCompactSizeWithLimit(s, KeyValueLimit); // map key
            std::vector<uint8_t> vData;
            s >> vData; // deserialize vector
            // We only care about 1 key currently, the peer version.
            // If we want to parse more keys, we may add extra if/else or switch clauses here.
            if (key == static_cast<uint64_t>(Key::Version)) {
                // Deserialize the data vector as a uint64_t data item -> VersionTuple
                VectorReader vr(SER_NETWORK, PROTOCOL_VERSION, vData, 0); // read from vector in-place
                values.version = VersionTuple::FromU64( ReadCompactSizeWithLimit(vr, KeyValueLimit) ); // may throw
                // Even after we read the 1 key we care about, we will keep
                // looping to deserialize and validate that the received
                // message is fully deserializable.
            }
        }
    }

    static void CheckSize(const size_t size) {
        if (size > MaxSize())
            throw std::ios_base::failure(
                    strprintf("An extversion message xmap must not exceed %d bytes", MaxSize()));
    }
};

} // namespace extversion
