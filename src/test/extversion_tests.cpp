// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Based on the xversion (renamed to extversion) subsystem from Bitcoin Unlimited
// Copyright (c) 2015-2020 The Bitcoin Unlimited developers
// License: MIT

#include <compat/endian.h>
#include <extversion.h>
#include <serialize.h>
#include <streams.h>
#include <test/setup_common.h>
#include <util/strencodings.h>
#include <version.h>

#include <boost/test/unit_test.hpp>

#include <cassert>
#include <cstdint>
#include <limits>
#include <map>
#include <random>
#include <tuple>
#include <utility>
#include <vector>

//! We will use a deterministic rng in this test, seeded with this value
static constexpr uint64_t seed = 0xfeed'f00d'1235'abcd;

BOOST_FIXTURE_TEST_SUITE(extversion_tests, BasicTestingSetup)

using extversion::VersionTuple;
using extversion::Message;

std::mt19937_64 g{seed};

auto gen(const uint64_t minimum, const uint64_t maximum) {
    assert(maximum >= minimum);
    return std::uniform_int_distribution<uint64_t>{minimum, maximum}(g);
}

BOOST_AUTO_TEST_CASE(version_tuple_basic) {
    using VT = VersionTuple;

    std::tuple<uint32_t, uint8_t, uint8_t, uint64_t> test_and_expected[] = {
        {       123,  12, 37,          1231237 },
        {         0,   0, 1,                 1 },
        {         0,   1, 0,               100 },
        {         1,   1, 1,             10101 },
        { 212345678,  99, 66, 2123456789966ULL },
    };
    for (const auto &tup : test_and_expected) {
        const auto maj = std::get<0>(tup);
        const auto min = std::get<1>(tup);
        const auto rev = std::get<2>(tup);
        const auto u64 = std::get<3>(tup);
        const VT vt{maj, min, rev};
        BOOST_CHECK_EQUAL(vt.Major(), maj);
        BOOST_CHECK_EQUAL(vt.Minor(), min);
        BOOST_CHECK_EQUAL(vt.Revision(), rev);
        BOOST_CHECK_EQUAL(vt.ToU64(), u64);
    }
}

using MajT = decltype(VersionTuple{}.Major());
using MinT = decltype(VersionTuple{}.Minor());
using RevT = decltype(VersionTuple{}.Revision());

BOOST_AUTO_TEST_CASE(version_tuple_u64_round_trip) {
    // Test to ensure that as long as we aren't using out-of-range minor/revision values,
    // everything is sane
    for (int i = 0; i < 4096; ++i) {
        constexpr auto lastNumberInRange = VersionTuple::MinorRevisionRange() - 1;
        const MajT maj = gen(0, std::numeric_limits<MajT>::max());
        const MinT min = gen(0, lastNumberInRange);
        const RevT rev = gen(0, lastNumberInRange);
        const auto vt = VersionTuple{maj, min, rev},
                   vt2 = VersionTuple::FromU64(vt.ToU64());
        BOOST_CHECK(vt == vt2);
        BOOST_CHECK_EQUAL(vt.ToU64(), vt2.ToU64());
        BOOST_CHECK_EQUAL(vt.Major(), maj);
        BOOST_CHECK_EQUAL(vt.Minor(), min);
        BOOST_CHECK_EQUAL(vt.Revision(), rev);
    }
}

BOOST_AUTO_TEST_CASE(version_tuple_out_of_range) {
    // Test to ensure that some out-of-range minor/revision values may lose information
    // (as is expected by the spec)
    for (int i = 0; i < 10000; ++i) {
        constexpr auto outOfRangeBegin = VersionTuple::MinorRevisionRange();
        const MajT maj = gen(0, std::numeric_limits<MajT>::max());
        const MinT min = gen(outOfRangeBegin, std::numeric_limits<MinT>::max());
        const RevT rev = gen(outOfRangeBegin, std::numeric_limits<RevT>::max());
        const auto vt = VersionTuple{maj, min, rev},
                   vt2 = VersionTuple::FromU64(vt.ToU64());
        BOOST_CHECK(vt != vt2);
    }
}

BOOST_AUTO_TEST_CASE(message_tests) {
    using Vec = std::vector<uint8_t>;
    using Map = std::map<uint8_t, Vec>;
    using MMap = std::multimap<uint8_t, Vec>;

    // Check that a ser/unser round-trip of Message works
    Vec vtmp;
    Message msg, msg2;
    msg.SetVersion({1, 2, 3});
    CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, vtmp, 0) << msg;
    VectorReader(SER_NETWORK, PROTOCOL_VERSION, vtmp, 0) >> msg2;
    BOOST_CHECK(msg.GetVersion() && msg2.GetVersion() && *msg.GetVersion() == *msg2.GetVersion());

    // Check that serializing a map -> unserializing as Message works
    // Note that we lack the compact map serializer so we use a map with
    // uint8_t keys to ensure it serializes like the BU compact map.
    static_assert (uint8_t(extversion::Key::Version) == uint64_t(extversion::Key::Version),
                   "Assumption is that Version key fits inside an 8-bit integer");
    Map m1{{
        { uint8_t(extversion::Key::Version), {100} },
    }};
    Vec v1;
    CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, v1, 0) << m1;
    BOOST_CHECK(v1.size() > 0);
    msg = Message();
    VectorReader(SER_NETWORK, PROTOCOL_VERSION, v1, 0) >> msg;

    BOOST_CHECK(bool(msg.GetVersion()));
    if (auto ver = msg.GetVersion())
        BOOST_CHECK(*ver == VersionTuple(0, 1, 0));

    // Check that serialize again is equal
    vtmp.clear();
    CVectorWriter(SER_NETWORK, PROTOCOL_VERSION, vtmp, 0) << msg;
    BOOST_CHECK(vtmp == v1);


    // Next, check the requirement that subsequent duplicate keys
    // in the map overwrite previous keys.
    const auto SerializeMMap = [](CVectorWriter &&vw, const MMap &m) {
        WriteCompactSize(vw, m.size());
        for (const auto &entry : m)
            Serialize(vw, entry);
    };
    MMap m2{{
        { uint8_t(extversion::Key::Version), {012} },
        { uint8_t(extversion::Key::Version), {123} },
        { uint8_t(extversion::Key::Version), {234} },
    }};
    Vec v2;
    SerializeMMap(CVectorWriter{SER_NETWORK, PROTOCOL_VERSION, v2, 0}, m2);
    BOOST_CHECK(v2.size() > 0);
    msg = Message();
    VectorReader(SER_NETWORK, PROTOCOL_VERSION, v2, 0) >> msg;

    BOOST_CHECK(bool(msg.GetVersion()));
    if (auto ver = msg.GetVersion())
        BOOST_CHECK(*ver == VersionTuple(0, 2, 34));


    // Next, try a map with below the key limit, at the key limit, and beyond the key limit
    // the first two should not throw, and the last should.
    using P = std::pair<uint16_t, bool>; // nKeys, shouldThrow
    BOOST_CHECK(Message::MaxNumKeys() < std::numeric_limits<uint16_t>::max());
    const P tests[] = {
        { Message::MaxNumKeys() - 1, false },
        { Message::MaxNumKeys(), false },
        { Message::MaxNumKeys() + 1, true },
    };
    for (auto & pair : tests) {
        const uint16_t nKeys = pair.first;
        const bool shouldThrow = pair.second;
        const uint16_t le16 = htole16(nKeys);
        const uint8_t *le8 = reinterpret_cast<const uint8_t *>(&le16);
        vtmp = {
            // Compact size (16-bit) for number of keys
            253, le8[0], le8[1],
        };
        static_assert (uint64_t(extversion::Key::Version) != 1, "This code assumes Version key is != 1");
        vtmp.resize(vtmp.size() + nKeys*3, 1); // each entry has minimum 3 bytes
        msg = Message();
        // IF shouldThrow == true, then this should throw, with "size too long" in the message
        try {
            VectorReader(SER_NETWORK, PROTOCOL_VERSION, vtmp, 0) >> msg;
            if (shouldThrow)
                throw std::runtime_error("Test failed -- expected Message deserialization to throw but it didn't");
            // shouldThrow == false case: all the keys didn't match Version, so Version should still be unset
            BOOST_CHECK(!msg.GetVersion());
        } catch (const std::ios_base::failure &e) {
            if (!shouldThrow || std::string(e.what()).find("size too large") == std::string::npos)
                throw;
        }
    }
}

BOOST_AUTO_TEST_SUITE_END()
