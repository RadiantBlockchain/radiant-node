// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/descriptor.h>
#include <script/sign.h>
#include <script/standard.h>
#include <util/strencodings.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

namespace {

void CheckUnparsable(const std::string &prv, const std::string &pub) {
    FlatSigningProvider keys_priv, keys_pub;
    auto parse_priv = Parse(prv, keys_priv);
    auto parse_pub = Parse(pub, keys_pub);
    BOOST_CHECK(!parse_priv);
    BOOST_CHECK(!parse_pub);
}

constexpr int DEFAULT = 0;
// Expected to be ranged descriptor
constexpr int RANGE = 1;
// Derivation needs access to private keys
constexpr int HARDENED = 2;
// This descriptor is not expected to be solvable
constexpr int UNSOLVABLE = 4;
// We can sign with this descriptor (this is not true when actual BIP32
// derivation is used, as that's not integrated in our signing code)
constexpr int SIGNABLE = 8;

std::string MaybeUseHInsteadOfApostrophy(std::string ret) {
    if (InsecureRandBool()) {
        while (true) {
            auto it = ret.find("'");
            if (it != std::string::npos) {
                ret[it] = 'h';
            } else {
                break;
            }
        }
    }
    return ret;
}

const std::set<std::vector<uint32_t>> ONLY_EMPTY{{}};

void Check(const std::string &prv, const std::string &pub, int flags,
           const std::vector<std::vector<std::string>> &scripts,
           const std::set<std::vector<uint32_t>> &paths = ONLY_EMPTY) {
    FlatSigningProvider keys_priv, keys_pub;
    std::set<std::vector<uint32_t>> left_paths = paths;

    // Check that parsing succeeds.
    auto parse_priv = Parse(MaybeUseHInsteadOfApostrophy(prv), keys_priv);
    auto parse_pub = Parse(MaybeUseHInsteadOfApostrophy(pub), keys_pub);
    BOOST_CHECK(parse_priv);
    BOOST_CHECK(parse_pub);

    // Check private keys are extracted from the private version but not the
    // public one.
    BOOST_CHECK(keys_priv.keys.size());
    BOOST_CHECK(!keys_pub.keys.size());

    // Check that both versions serialize back to the public version.
    std::string pub1 = parse_priv->ToString();
    std::string pub2 = parse_pub->ToString();
    BOOST_CHECK_EQUAL(pub, pub1);
    BOOST_CHECK_EQUAL(pub, pub2);

    // Check that both can be serialized with private key back to the private
    // version, but not without private key.
    std::string prv1, prv2;
    BOOST_CHECK(parse_priv->ToPrivateString(keys_priv, prv1));
    BOOST_CHECK_EQUAL(prv, prv1);
    BOOST_CHECK(!parse_priv->ToPrivateString(keys_pub, prv1));
    BOOST_CHECK(parse_pub->ToPrivateString(keys_priv, prv1));
    BOOST_CHECK_EQUAL(prv, prv1);
    BOOST_CHECK(!parse_pub->ToPrivateString(keys_pub, prv1));

    // Check whether IsRange on both returns the expected result
    BOOST_CHECK_EQUAL(parse_pub->IsRange(), (flags & RANGE) != 0);
    BOOST_CHECK_EQUAL(parse_priv->IsRange(), (flags & RANGE) != 0);

    // Is not ranged descriptor, only a single result is expected.
    if (!(flags & RANGE)) {
        assert(scripts.size() == 1);
    }

    auto const null_context = std::nullopt;

    size_t max = (flags & RANGE) ? scripts.size() : 3;
    for (size_t i = 0; i < max; ++i) {
        const auto &ref = scripts[(flags & RANGE) ? i : 0];
        for (int t = 0; t < 2; ++t) {
            const FlatSigningProvider &key_provider =
                (flags & HARDENED) ? keys_priv : keys_pub;
            FlatSigningProvider script_provider;
            std::vector<CScript> spks;
            BOOST_CHECK((t ? parse_priv : parse_pub)
                            ->Expand(i, key_provider, spks, script_provider));
            BOOST_CHECK_EQUAL(spks.size(), ref.size());
            for (size_t n = 0; n < spks.size(); ++n) {
                BOOST_CHECK_EQUAL(ref[n], HexStr(spks[n]));

                BOOST_CHECK_EQUAL(
                    IsSolvable(Merge(key_provider, script_provider), spks[n], null_context),
                    (flags & UNSOLVABLE) == 0);

                if (flags & SIGNABLE) {
                    CMutableTransaction spend;
                    spend.vin.resize(1);
                    spend.vout.resize(1);
                    BOOST_CHECK_MESSAGE(
                        SignSignature(Merge(keys_priv, script_provider),
                                      spks[n], spend, 0, 1 * COIN,
                                      SigHashType().withForkId(), null_context),
                        prv);
                }
            }
            // Test whether the observed key path is present in the 'paths'
            // variable (which contains expected, unobserved paths), and then
            // remove it from that set.
            for (const auto &origin : script_provider.origins) {
                BOOST_CHECK_MESSAGE(paths.count(origin.second.path),
                                    "Unexpected key path: " + prv);
                left_paths.erase(origin.second.path);
            }
        }
    }
    // Verify no expected paths remain that were not observed.
    BOOST_CHECK_MESSAGE(left_paths.empty(),
                        "Not all expected key paths found: " + prv);
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(descriptor_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(descriptor_test) {
    // Basic single-key compressed
    Check("combo(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
          "combo("
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)",
          SIGNABLE,
          {{"2103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5"
            "bdac",
            "76a9149a1c78a507689f6f54b847ad1cef1e614ee23f1e88ac"}});
    Check("pk(L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
          "pk("
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)",
          SIGNABLE,
          {{"2103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5"
            "bdac"}});
    Check("pkh([deadbeef/1/2'/3/4']"
          "L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1)",
          "pkh([deadbeef/1/2'/3/4']"
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd)",
          SIGNABLE, {{"76a9149a1c78a507689f6f54b847ad1cef1e614ee23f1e88ac"}},
          {{1, 0x80000002UL, 3, 0x80000004UL}});

    // Basic single-key uncompressed
    Check(
        "combo(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
        "combo("
        "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b8d"
        "ec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
        SIGNABLE,
        {{"4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd"
          "5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235ac",
          "76a914b5bd079c4d57cc7fc28ecf8213a6b791625b818388ac"}});
    Check("pk(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
          "pk("
          "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b"
          "8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
          SIGNABLE,
          {{"4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5"
            "bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235"
            "ac"}});
    Check("pkh(5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
          "pkh("
          "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b"
          "8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
          SIGNABLE, {{"76a914b5bd079c4d57cc7fc28ecf8213a6b791625b818388ac"}});
 
    // Versions with BIP32 derivations
    Check("combo([01234567]"
          "xprvA1RpRA33e1JQ7ifknakTFpgNXPmW2YvmhqLQYMmrj4xJXXWYpDPS3xz7iAxn8L39"
          "njGVyuoseXzU6rcxFLJ8HFsTjSyQbLYnMpCqE2VbFWc)",
          "combo([01234567]"
          "xpub6ERApfZwUNrhLCkDtcHTcxd75RbzS1ed54G1LkBUHQVHQKqhMkhgbmJbZRkrgZw4"
          "koxb5JaHWkY4ALHY2grBGRjaDMzQLcgJvLJuZZvRcEL)",
          SIGNABLE,
          {{"2102d2b36900396c9282fa14628566582f206a5dd0bcc8d5e892611806cafb0301"
            "f0ac",
            "76a91431a507b815593dfc51ffc7245ae7e5aee304246e88ac"}});
    Check("pk("
          "xprv9uPDJpEQgRQfDcW7BkF7eTya6RPxXeJCqCJGHuCJ4GiRVLzkTXBAJMu2qaMWPrS7"
          "AANYqdq6vcBcBUdJCVVFceUvJFjaPdGZ2y9WACViL4L/0)",
          "pk("
          "xpub68NZiKmJWnxxS6aaHmn81bvJeTESw724CRDs6HbuccFQN9Ku14VQrADWgqbhhTHB"
          "aohPX4CjNLf9fq9MYo6oDaPPLPxSb7gwQN3ih19Zm4Y/0)",
          DEFAULT,
          {{"210379e45b3cf75f9c5f9befd8e9506fb962f6a9d185ac87001ec44a8d3df8d4a9"
            "e3ac"}},
          {{0}});
    Check("pkh("
          "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssr"
          "dK4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U/2147483647'/0)",
          "pkh("
          "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6o"
          "DMSgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB/2147483647'/0)",
          HARDENED, {{"76a914ebdc90806a9c4356c1c88e42216611e1cb4c1c1788ac"}},
          {{0xFFFFFFFFUL, 0}});
    Check("combo("
          "xprvA2JDeKCSNNZky6uBCviVfJSKyQ1mDYahRjijr5idH2WwLsEd4Hsb2Tyh8RfQMuPh"
          "7f7RtyzTtdrbdqqsunu5Mm3wDvUAKRHSC34sJ7in334/*)",
          "combo("
          "xpub6FHa3pjLCk84BayeJxFW2SP4XRrFd1JYnxeLeU8EqN3vDfZmbqBqaGJAyiLjTAwm"
          "6ZLRQUMv1ZACTj37sR62cfN7fe5JnJ7dh8zL4fiyLHV/*)",
          RANGE,
          {{"2102df12b7035bdac8e3bab862a3a83d06ea6b17b6753d52edecba9be46f5d09e0"
            "76ac",
            "76a914f90e3178ca25f2c808dc76624032d352fdbdfaf288ac"},
           {"21032869a233c9adff9a994e4966e5b821fd5bac066da6c3112488dc52383b4a98"
            "ecac",
            "76a914a8409d1b6dfb1ed2a3e8aa5e0ef2ff26b15b75b788ac"}},
          {{0}, {1}});
    // BIP 32 path element overflow
    CheckUnparsable(
        "pkh("
        "xprv9s21ZrQH143K31xYSDQpPDxsXRTUcvj2iNHm5NUtrGiGG5e2DtALGdso3pGz6ssrdK"
        "4PFmM8NSpSBHNqPqm55Qn3LqFtT2emdEXVYsCzC2U/2147483648)",
        "pkh("
        "xpub661MyMwAqRbcFW31YEwpkMuc5THy2PSt5bDMsktWQcFF8syAmRUapSCGu8ED9W6oDM"
        "Sgv6Zz8idoc4a6mr8BDzTJY47LJhkJ8UB7WEGuduB/2147483648)");

    // Multisig constructions
    Check("multi(1,L4rK1yDtCWekvXuE6oXD9jCYfFNV2cWRpVuPLBcCU2z8TrisoyY1,"
          "5KYZdUEo39z3FPrtuX2QbbwGnNP5zTd7yyr2SC1j299sBCnWjss)",
          "multi(1,"
          "03a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd,"
          "04a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540c5bd5b"
          "8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abea235)",
          SIGNABLE,
          {{"512103a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c540"
            "c5bd4104a34b99f22c790c4e36b2b3c2c35a36db06226e41c692fc82b8b56ac1c5"
            "40c5bd5b8dec5235a0fa8722476c7709c02559e3aa73aa03918ba2d492eea75abe"
            "a23552ae"}});
     
}

BOOST_AUTO_TEST_SUITE_END()
