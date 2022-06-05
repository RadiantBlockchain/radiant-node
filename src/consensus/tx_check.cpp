// Copyright (c) 2017-2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <version.h>

static bool CheckTransactionCommon(const CTransaction &tx,
                                   CValidationState &state) {
    // Basic checks that don't depend on any context
    if (tx.vin.empty()) {
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vin-empty");
    }

    if (tx.vout.empty()) {
        return state.DoS(10, false, REJECT_INVALID, "bad-txns-vout-empty");
    }

    // Size limit
    if (::GetSerializeSize(tx, PROTOCOL_VERSION) > MAX_TX_SIZE) {
        return state.DoS(100, false, REJECT_INVALID, "bad-txns-oversize");
    }

    // Check for negative or overflow output values
    Amount nValueOut = Amount::zero();
    for (const auto &txout : tx.vout) {
        if (txout.nValue < Amount::zero()) {
            return state.DoS(100, false, REJECT_INVALID,
                             "bad-txns-vout-negative");
        }

        if (txout.nValue > MAX_MONEY) {
            return state.DoS(100, false, REJECT_INVALID,
                             "bad-txns-vout-toolarge");
        }

        nValueOut += txout.nValue;
        if (!MoneyRange(nValueOut)) {
            return state.DoS(100, false, REJECT_INVALID,
                             "bad-txns-txouttotal-toolarge");
        }
    }

    return true;
}

bool CheckCoinbase(const CTransaction &tx, CValidationState &state) {
    if (!tx.IsCoinBase()) {
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-missing", false,
                         "first tx is not coinbase");
    }

    if (!CheckTransactionCommon(tx, state)) {
        // CheckTransactionCommon fill in the state.
        return false;
    }

    if (tx.vin[0].scriptSig.size() < 1 ||
        tx.vin[0].scriptSig.size() > MAX_COINBASE_SCRIPTSIG_SIZE) {
        return state.DoS(100, false, REJECT_INVALID, "bad-cb-length");
    }

    return true;
}

bool CheckRegularTransaction(const CTransaction &tx, CValidationState &state) {
    if (tx.IsCoinBase()) {
        return state.DoS(100, false, REJECT_INVALID, "bad-tx-coinbase");
    }

    if (!CheckTransactionCommon(tx, state)) {
        // CheckTransactionCommon fill in the state.
        return false;
    }

    // Check for duplicate inputs.
    // Simply checking every pair is O(n^2).
    // Sorting a vector and checking adjacent elements is O(n log n).
    // However, the vector requires a memory allocation, copying and sorting.
    // This is significantly slower for small transactions. The crossover point
    // was measured to be a vin.size() of about 120 on x86-64.
    if (tx.vin.size() < 120) {
        for (size_t i = 0; i < tx.vin.size(); ++i) {
            if (tx.vin[i].prevout.IsNull()) {
                return state.DoS(10, false, REJECT_INVALID,
                                 "bad-txns-prevout-null");
            }
            for (size_t j = i + 1; j < tx.vin.size(); ++j) {
                if (tx.vin[i].prevout == tx.vin[j].prevout) {
                    return state.DoS(100, false, REJECT_INVALID, "bad-txns-inputs-duplicate");
                }
            }
        }
    } else {
        std::vector<const COutPoint*> sortedPrevOuts(tx.vin.size());
        for (size_t i = 0; i < tx.vin.size(); ++i) {
            if (tx.vin[i].prevout.IsNull()) {
                return state.DoS(10, false, REJECT_INVALID,
                                 "bad-txns-prevout-null");
            }
            sortedPrevOuts[i] = &tx.vin[i].prevout;
        }
        std::sort(sortedPrevOuts.begin(), sortedPrevOuts.end(), [](const COutPoint *a, const COutPoint *b) {
            return *a < *b;
        });
        auto it = std::adjacent_find(sortedPrevOuts.begin(), sortedPrevOuts.end(), [](const COutPoint *a, const COutPoint *b) {
            return *a == *b;
        });
        if (it != sortedPrevOuts.end()) {
            return state.DoS(100, false, REJECT_INVALID,
                             "bad-txns-inputs-duplicate");
        }
    }
    return true;
}
