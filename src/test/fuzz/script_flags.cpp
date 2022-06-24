// Copyright (c) 2009-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/interpreter.h>
#include <script/script.h>
#include <streams.h>
#include <version.h>

#include <test/fuzz/fuzz.h>

/** Flags that are not forbidden by an assert */
static bool IsValidFlagCombination(uint32_t flags);

void test_one_input(std::vector<uint8_t> buffer) {
    CDataStream ds(buffer, SER_NETWORK, INIT_PROTO_VERSION);
    try {
        int nVersion;
        ds >> nVersion;
        ds.SetVersion(nVersion);
    } catch (const std::ios_base::failure &) {
        return;
    }

    try {
        const CTransaction tx(deserialize, ds);
        const PrecomputedTransactionData txdata(tx);

        uint32_t verify_flags;
        ds >> verify_flags;

        if (!IsValidFlagCombination(verify_flags)) {
            return;
        }

        uint32_t fuzzed_flags;
        ds >> fuzzed_flags;

        for (unsigned int i = 0; i < tx.vin.size(); ++i) {
            CTxOut prevout;
            ds >> prevout;

            const TransactionSignatureChecker checker{&tx, i, prevout.nValue,
                                                      txdata};

            ScriptError serror;
            auto const null_context = std::nullopt;
            const bool ret =
                VerifyScript(tx.vin.at(i).scriptSig, prevout.scriptPubKey,
                             verify_flags, checker, null_context, &serror);
            assert(ret == (serror == ScriptError::OK));

            // Verify that removing flags from a passing test or adding flags to
            // a failing test does not change the result
            if (ret) {
                verify_flags &= ~fuzzed_flags;
            } else {
                verify_flags |= fuzzed_flags;
            }
            if (!IsValidFlagCombination(verify_flags)) {
                return;
            }

            ScriptError serror_fuzzed;
            const bool ret_fuzzed =
                VerifyScript(tx.vin.at(i).scriptSig, prevout.scriptPubKey,
                             verify_flags, checker, null_context, &serror_fuzzed);
            assert(ret_fuzzed == (serror_fuzzed == ScriptError::OK));

            assert(ret_fuzzed == ret);
        }
    } catch (const std::ios_base::failure &) {
        return;
    }
}

static bool IsValidFlagCombination(uint32_t flags) {
    // All combinations are valid
    return true;
}
