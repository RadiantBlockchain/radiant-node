// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SCRIPT_SCRIPT_EXECUTION_CONTEXT_H
#define BITCOIN_SCRIPT_SCRIPT_EXECUTION_CONTEXT_H

#include <coins.h>
#include <primitives/transaction.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

struct PSBTInput;

/// An execution context for evaluating a script input. Note that this object contains some shared
/// data that is shared for all inputs to a tx. This object is given to CScriptCheck as well
/// as passed down to VerifyScript() and friends (for native introspection).
///
/// NOTE: In all cases below, the referenced transaction, `tx`, must remain valid throughout this
/// object's lifetime!
class ScriptExecutionContext {
    unsigned nIn{}; ///< the input number being evaluated. This is an index into shared->tx.vin
    bool limited{}; ///< if true, we lack a valid "Coin" for anything but shared->inputCoins[nIn]

    /// All the inputs in a tx share this object
    struct Shared {
        /// All coins being spent by inputs to this transaction.
        /// The size of this vector is the same as tx.vin().size()
        std::vector<Coin> inputCoins;
        /// The transaction being evaluated.
        CTransactionView tx;

        // For std::make_shared to work correctly.
        Shared(std::vector<Coin> && coins, CTransactionView tx_)
            : inputCoins(std::move(coins)), tx(tx_) {}
    };

    using SharedPtr = std::shared_ptr<const Shared>;

    SharedPtr shared;

    /// Construct a specific context for this input, given a tx.
    /// Use this constructor for the first input in a tx.
    /// All of the coins for the tx will get pre-cached and a new internal Shared object will be constructed.
    ScriptExecutionContext(unsigned input, const CCoinsViewCache &coinsCache, CTransactionView tx);

    /// Construct a specific context for this input, given a tx.
    /// Use this constructor for the first input in a tx.
    /// All of the coins for the tx will get pre-cached and a new internal Shared object will be constructed.
    ScriptExecutionContext(unsigned input, const std::vector<PSBTInput> &inputs, CTransactionView tx);

    /// Construct a specific context for this input, given another context.
    /// The two contexts will share the same Shared data.
    /// Use this constructor for all subsequent inputs to a tx (so that they may all share the same context)
    ScriptExecutionContext(unsigned input, const ScriptExecutionContext &sharedContext);

public:
    /// Factory method to create a context for all inputs in a tx.
    static
    std::vector<ScriptExecutionContext> createForAllInputs(CTransactionView tx, const CCoinsViewCache &coinsCache);

    /// Like the above, but takes a partially-signed bitcoin transacton's inputs as its coin source.
    static
    std::vector<ScriptExecutionContext> createForAllInputs(CTransactionView tx, const std::vector<PSBTInput> &inputs);

    /// Construct a *limited* context that cannot see all coins (utxos). It only has the coin for this input.
    /// All other sibling input coins will appear as coin.IsSpent() (null data).  this->isLimited() will return
    /// true if this constructor is used.
    ///
    /// This constructor is intended for tests or for situations where only limited introspection is available,
    /// such as signing a tx where we don't have a view of all the extant input coins.
    ScriptExecutionContext(unsigned input, const CScript &scriptPubKey, Amount amount, CTransactionView tx,
                           /* NOTE: The below two properties of coins are ignored by the script interpreter, so
                              they need not be specified. If that becomes untrue, update this code to require
                              caller to specify them appropriately. */
                           uint32_t nHeight = 1, bool isCoinbase = false);

    /// Get the input number being evaluated
    unsigned inputIndex() const { return nIn; }

    /// If true, we lack a valid "Coin" for anything but the input corresponding to inputIndex().
    /// This affects OP-codes that match: OP_UTXO*.
    ///
    /// If this is true, then coin(), coinScriptPubKey() and coinAmount() will return unspecified
    /// values for anything but this input's inputIndex().
    ///
    /// (The "limited" usage of this class is for tests or other non-consensus-critical code).
    bool isLimited() const { return limited; }

    /// Get a reference to the coin being spent for input at index `inputIdx`.
    ///
    /// Returned coin may be be IsSpent() if coin is missing (such as in a *limited* context
    /// where all sibling coin info is unavailable).
    const Coin & coin(std::optional<unsigned> inputIdx = {}) const {
        // Defensive programming: A class invariant is that the number of coins equals the number of
        // inputs in tx().vin().  However, we use .at() here in case the underlying tx is mutable and
        // is being misused.. and it mutates such that there are now more inputs in the tx than there
        // are coins in our coins view.
        return shared->inputCoins.at(inputIdx.value_or(nIn));
    }

    /// Get the coin (utxo) scriptPubKey for this input or any input.
    ///
    /// Note that if `isLimited()`, this method only returns a valid value for this input.
    const CScript & coinScriptPubKey(std::optional<unsigned> inputIdx = {}) const {
        return coin(inputIdx).GetTxOut().scriptPubKey;
    }

    /// Get the amount for this input or any input.
    ///
    /// Note that if `isLimited()`, this method only returns a valid value for this input.
    const Amount & coinAmount(std::optional<unsigned> inputIdx = {}) const {
        return coin(inputIdx).GetTxOut().nValue;
    }

    /// Get the scriptSig for this input or any input (tx().vin[i].scriptSig)
    const CScript & scriptSig(std::optional<unsigned> inputIdx = {}) const {
         return tx().vin().at(inputIdx.value_or(nIn)).scriptSig;
    }

    /// The transaction associated with this script evaluation context.
    const CTransactionView & tx() const { return shared->tx; }
};

using ScriptExecutionContextOpt = std::optional<ScriptExecutionContext>;

#endif // BITCOIN_SCRIPT_SCRIPT_EXECUTION_CONTEXT_H
