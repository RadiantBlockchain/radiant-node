// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <coins.h>
#include <primitives/transaction.h>

#include <memory>
#include <optional>
#include <utility>
#include <vector>

struct PSBTInput;

/**
 * @brief Radiant state for push input reference data extracted from a script.
 * 
 */
struct PushRefScriptSummary {
    Amount nValue;
    std::set<uint288> pushRefSet;
    std::set<uint288> requireRefSet;
    std::set<uint288> disallowSiblingRefSet;
    std::set<uint288> singletonRefSet;
    uint256 codeScriptHash;
    uint32_t stateSeperatorByteIndex;
};

/// An execution context for evaluating a script input. Note that this object contains some shared
/// data that is shared for all inputs to a tx. This object is given to CScriptCheck as well
/// as passed down to VerifyScript() and friends (for native introspection).
///
/// NOTE: In all cases below, the referenced transaction, `tx`, must remain valid throughout this
/// object's lifetime!
#if defined(__GNUG__) && !defined(__clang__)
  // silence this warning for g++ only - known compiler bug, see https://gcc.gnu.org/bugzilla/show_bug.cgi?id=102801 and https://gcc.gnu.org/bugzilla/show_bug.cgi?id=80635
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif // defined(__GNUG__) && !defined(__clang__)
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

        /// Extended shared context for Radiant
        /// 
        ///
        std::vector<PushRefScriptSummary> vectorInputsPushRefScriptSummary;
        std::vector<PushRefScriptSummary> vectorOutputsPushRefScriptSummary;

        std::set<uint288> inputPushRefSet;
        std::set<uint288> outputPushRefSet;
        std::set<uint288> inputSingletonRefSet;
        std::set<uint288> outputSingletonRefSet;

        // Metrics by refHash
        // Total satoshi amount
        std::map<uint256, Amount> inputRefHashToAmountMap;
        std::map<uint256, Amount> outputRefHashToAmountMap;
        std::vector<uint256> inputIndexToRefHashDataSummaryHash;
        std::vector<uint256> outputIndexRefHashToDataSummaryHash;

        // Metrics by refAssetId
        std::map<uint288, Amount> inputRefAssetIdToAmountMap;
        std::map<uint288, Amount> outputRefAssetIdToAmountMap;
        std::map<uint288, uint32_t> inputRefAssetIdToOutputCountMap;
        std::map<uint288, uint32_t> outputRefAssetIdToOutputCountMap;
        std::map<uint288, uint32_t> inputRefAssetIdToZeroValueOutputCountMap;
        std::map<uint288, uint32_t> outputRefAssetIdToZeroValueOutputCountMap;

        // Metrics by codeScriptHash
        std::map<uint256, Amount> inputCodeScriptHashToAmountMap;
        std::map<uint256, Amount> outputCodeScriptHashToAmountMap;
        std::map<uint256, uint32_t> inputCodeScriptHashToOutputCountMap;
        std::map<uint256, uint32_t> outputCodeScriptHashToOutputCountMap;
        std::map<uint256, uint32_t> inputCodeScriptHashToZeroValueOutputCountMap;
        std::map<uint256, uint32_t> outputCodeScriptHashToZeroValueOutputCountMap;
        // State Seperator map
        std::map<uint32_t, uint32_t> inputIndexToStateSeperatorIndex;
        std::map<uint32_t, uint32_t> outputIndexToStateSeperatorIndex;
        ///
        ///
        /// End of Extended shared context for Radiant
 
        // For std::make_shared to work correctly.
        Shared(std::vector<Coin> && coins, CTransactionView tx_)
            : inputCoins(std::move(coins)), tx(tx_) {
            initializeInputSummary();
            initializeOutputSummary();
        }

        /// Radiant specific extended context
        void initializeInputSummary(){
 
            for (uint64_t i = 0; i < inputCoins.size(); i++) {
                auto const& coin = inputCoins.at(i);
                auto const& utxoScript = coin.GetTxOut().scriptPubKey;
                auto const& nValue = coin.GetTxOut().nValue;

                calculatePrecomputedStateForScript(
                    utxoScript,
                    nValue,
                    i,
                    inputPushRefSet,
                    inputSingletonRefSet,
                    vectorInputsPushRefScriptSummary,
                    inputIndexToRefHashDataSummaryHash,
                    inputRefHashToAmountMap,
                    inputRefAssetIdToAmountMap,
                    inputRefAssetIdToOutputCountMap,
                    inputRefAssetIdToZeroValueOutputCountMap,
                    inputCodeScriptHashToAmountMap,
                    inputCodeScriptHashToOutputCountMap,
                    inputCodeScriptHashToZeroValueOutputCountMap,
                    inputIndexToStateSeperatorIndex
                );
            }
        }

        void initializeOutputSummary(){

            for (uint64_t i = 0; i < tx.vout().size(); i++) {
                auto const& coinOutput = tx.vout()[i];
                auto const& outputScript = coinOutput.scriptPubKey;
                auto const& nValue = coinOutput.nValue;

                calculatePrecomputedStateForScript(
                    outputScript,
                    nValue,
                    i,
                    outputPushRefSet,
                    outputSingletonRefSet,
                    vectorOutputsPushRefScriptSummary,
                    outputIndexRefHashToDataSummaryHash,
                    outputRefHashToAmountMap,
                    outputRefAssetIdToAmountMap,
                    outputRefAssetIdToOutputCountMap,
                    outputRefAssetIdToZeroValueOutputCountMap,
                    outputCodeScriptHashToAmountMap,
                    outputCodeScriptHashToOutputCountMap,
                    outputCodeScriptHashToZeroValueOutputCountMap,
                    outputIndexToStateSeperatorIndex
                );
            }
        }

        static void calculatePrecomputedStateForScript(
            const CScript &script,
            const Amount &nValue,
            uint64_t index,
            std::set<uint288> &globalPushRefSet,
            std::set<uint288> &singletonRefSet,
            std::vector<PushRefScriptSummary> &vectorPushRefScriptSummary,
            std::vector<uint256> &indexToDataSummaryHash,
            std::map<uint256, Amount> &refHashToAmountMap,
            std::map<uint288, Amount> &refAssetIdToAmountMap,
            std::map<uint288, uint32_t> &refAssetIdToOutputCountMap,
            std::map<uint288, uint32_t> &refAssetIdToZeroValueOutputCountMap,
            std::map<uint256, Amount> &codeScriptHashToAmountMap,
            std::map<uint256, uint32_t> &codeScriptHashToOutputCountMap,
            std::map<uint256, uint32_t> &codeScriptHashToZeroValueOutputCountMap,
            std::map<uint32_t, uint32_t> &indexToStateSeperatorIndex 
        ) {
            uint256 zeroRefHash(uint256S("0000000000000000000000000000000000000000000000000000000000000000"));
            uint288 zeroRefAssetId(uint288S("000000000000000000000000000000000000000000000000000000000000000000000000"));
            // Step 1. Populate the push ref information 
            std::set<uint288> pushRefSetLocal;
            std::set<uint288> requireRefSetLocal;
            std::set<uint288> disallowSiblingRefSetLocal;
            std::set<uint288> singletonRefSetLocal;
            uint32_t stateSeperatorByteIndex;
            if (!script.GetPushRefs(pushRefSetLocal, requireRefSetLocal, disallowSiblingRefSetLocal, singletonRefSetLocal, stateSeperatorByteIndex)) {
                // Fatal error parsing output should never happen
                throw std::runtime_error("Error: script-execution-context-init");
            }

            PushRefScriptSummary scriptSummary;
            scriptSummary.nValue = nValue;
            scriptSummary.pushRefSet = pushRefSetLocal;
            scriptSummary.requireRefSet = requireRefSetLocal;
            scriptSummary.disallowSiblingRefSet = disallowSiblingRefSetLocal;
            scriptSummary.singletonRefSet = singletonRefSetLocal;
            scriptSummary.stateSeperatorByteIndex = stateSeperatorByteIndex;

            // Merge in the local pushRefSet and singletonRefSet
            globalPushRefSet.insert(pushRefSetLocal.begin(), pushRefSetLocal.end());
            singletonRefSet.insert(singletonRefSetLocal.begin(), singletonRefSetLocal.end());
            // Populate state seperator map
            // Serves:
            // 
            // - OP_STATESEPARATOR
            // - <inputIndex> OP_STATESEPARATORINDEX_UTXO
            // - <outputIndex> OP_STATESEPARATORINDEX_OUTPUT
            indexToStateSeperatorIndex.insert(std::pair(index, stateSeperatorByteIndex));
            // Populate the maps for refHash
            // Serves:
            //
            // - <refHash> OP_REFHASHVALUESUM_UTXOS
            // - <refHash> OP_REFHASHVALUESUM_OUTPUTS
            // - <refHash> OP_REFHASHDATASUMMARY_UTXO
            // - <refHash> OP_REFHASHDATASUMMARY_OUTPUT 
            RefHashDataSummary outputSummary = getRefHashDataSummary(pushRefSetLocal, script, nValue, zeroRefHash);
            auto refsIt = refHashToAmountMap.find(outputSummary.refsHash);
            if (refsIt == refHashToAmountMap.end()) {
                // If it doesn't exist, then just initialize it
                refHashToAmountMap.insert(std::pair(outputSummary.refsHash, Amount::zero()));
                refsIt = refHashToAmountMap.find(outputSummary.refsHash);
            }
            refsIt->second += nValue;
            CHashWriter hashOutputDataSummaryWriter(SER_GETHASH, 0);
            hashOutputDataSummaryWriter << outputSummary.nValue;
            hashOutputDataSummaryWriter << outputSummary.scriptPubKeyHash;
            hashOutputDataSummaryWriter << outputSummary.totalRefs;
            hashOutputDataSummaryWriter << outputSummary.refsHash;
            indexToDataSummaryHash.push_back(hashOutputDataSummaryWriter.GetHash());
            // Populate the maps for refAssetId
            // Serves:
            //
            // Information by refAssetId
            // - <refAssetId 36 bytes> OP_REFVALUESUM_UTXOS
            // - <refAssetId 36 bytes> OP_REFVALUESUM_OUTPUTS
            // - <refAssetId 36 bytes> OP_REFOUTPUTCOUNT_UTXOS
            // - <refAssetId 36 bytes> OP_REFOUTPUTCOUNT_OUTPUTS
            // - <refAssetId 36 bytes> OP_REFOUTPUTCOUNTZEROVALUED_UTXOS
            // - <refAssetId 36 bytes> OP_REFOUTPUTCOUNTZEROVALUED_OUTPUTS
            // - <inputIndex> OP_REFDATASUMMARY_UTXO
            // - <outputIndex> OP_REFDATASUMMARY_OUTPUT
            // Initial the map of refAssetId to amount
            // First insert the zeroRefHash if there are no push refs
            if (pushRefSetLocal.size() == 0) {
                refAssetIdToAmountMap.insert(std::pair(zeroRefAssetId, nValue));
            } else {
                // There is at least one push reference, 
                std::set<uint288>::const_iterator pushRefsIt;
                for (pushRefsIt = pushRefSetLocal.begin(); pushRefsIt != pushRefSetLocal.end(); pushRefsIt++) {
                    // Sum the total value of the outputs
                    auto foundPushRefMapIt = refAssetIdToAmountMap.find(*pushRefsIt);
                    if (foundPushRefMapIt == refAssetIdToAmountMap.end()) {
                        // If it doesn't exist, then just initialize it
                        refAssetIdToAmountMap.insert(std::pair(*pushRefsIt, Amount::zero()));
                        foundPushRefMapIt = refAssetIdToAmountMap.find(*pushRefsIt);
                    }
                    foundPushRefMapIt->second += nValue;
                    
                    //  Count the number of outputs that exist for the given assetId
                    auto foundRefAssetIdToOutputCountMapIt = refAssetIdToOutputCountMap.find(*pushRefsIt);
                    if (foundRefAssetIdToOutputCountMapIt == refAssetIdToOutputCountMap.end()) {
                        // If it doesn't exist, then just initialize it
                        refAssetIdToOutputCountMap.insert(std::pair(*pushRefsIt, 0));
                        foundRefAssetIdToOutputCountMapIt = refAssetIdToOutputCountMap.find(*pushRefsIt);
                    }
                    foundRefAssetIdToOutputCountMapIt->second++;

                    //  Count the number of zero valued outputs that exist for the given assetId
                    auto foundRefAssetIdToZeroValueOutputCountMapIt = refAssetIdToZeroValueOutputCountMap.find(*pushRefsIt);
                    if (foundRefAssetIdToZeroValueOutputCountMapIt == refAssetIdToZeroValueOutputCountMap.end()) {
                        // If it doesn't exist, then just initialize it
                        refAssetIdToZeroValueOutputCountMap.insert(std::pair(*pushRefsIt, 0));
                        foundRefAssetIdToZeroValueOutputCountMapIt = refAssetIdToZeroValueOutputCountMap.find(*pushRefsIt);
                    }
                    if (nValue == Amount::zero()) {
                        foundRefAssetIdToZeroValueOutputCountMapIt->second++;
                    }
                }
            }
            // Create the codeScriptHash
            if (stateSeperatorByteIndex >= script.size()) {
                CHashWriter hashWriterCodeScriptHashWriter(SER_GETHASH, 0);
                hashWriterCodeScriptHashWriter << CFlatData(CScript(script.end(), script.end()));
                scriptSummary.codeScriptHash = hashWriterCodeScriptHashWriter.GetHash(); 
            } else {
                CScript::const_iterator scriptStateSeperatorIterator = script.begin() + stateSeperatorByteIndex; 
                CHashWriter hashWriterCodeScriptHashWriter(SER_GETHASH, 0);
                hashWriterCodeScriptHashWriter << CFlatData(CScript(scriptStateSeperatorIterator, script.end()));
                scriptSummary.codeScriptHash = hashWriterCodeScriptHashWriter.GetHash(); 
            }
            vectorPushRefScriptSummary.push_back(scriptSummary);
            // Populate the maps for codeScriptHash
            // Serves:
            //
            // Information by codeScriptHash
            // - <codeScriptHash 32 bytes> OP_CODESCRIPTHASHVALUESUM_UTXOS
            // - <codeScriptHash 32 bytes> OP_CODESCRIPTHASHVALUESUM_OUTPUTS
            // - <codeScriptHash 32 bytes> OP_CODESCRIPTHASHOUTPUTCOUNT_UTXOS
            // - <codeScriptHash 32 bytes> OP_CODESCRIPTHASHOUTPUTCOUNT_OUTPUTS
            // - <codeScriptHash 32 bytes> OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_UTXOS
            // - <codeScriptHash 32 bytes> OP_CODESCRIPTHASHZEROVALUEDOUTPUTCOUNT_OUTPUTS
            // - <inputIndex> OP_CODESCRIPTHASH_UTXO
            // - <outputIndex> OP_CODESCRIPTHASH_OUTPUT
            // Sum the total value of the outputs
            auto foundCodeScriptToAmountIt = codeScriptHashToAmountMap.find(scriptSummary.codeScriptHash);
            if (foundCodeScriptToAmountIt == codeScriptHashToAmountMap.end()) {
                // If it doesn't exist, then just initialize it
                codeScriptHashToAmountMap.insert(std::pair(scriptSummary.codeScriptHash, Amount::zero()));
                foundCodeScriptToAmountIt = codeScriptHashToAmountMap.find(scriptSummary.codeScriptHash);
            }
            foundCodeScriptToAmountIt->second += nValue;
            
            //  Count the number of outputs that exist for the given assetId
            auto foundCodeScriptToOutputCountIt = codeScriptHashToOutputCountMap.find(scriptSummary.codeScriptHash);
            if (foundCodeScriptToOutputCountIt == codeScriptHashToOutputCountMap.end()) {
                // If it doesn't exist, then just initialize it
                codeScriptHashToOutputCountMap.insert(std::pair(scriptSummary.codeScriptHash, 0));
                foundCodeScriptToOutputCountIt = codeScriptHashToOutputCountMap.find(scriptSummary.codeScriptHash);
            }
            foundCodeScriptToOutputCountIt->second++;

            //  Count the number of zero valued outputs that exist for the given codeScriptHash
            auto foundCodeScriptToZeroValueOutputCountMapIt = codeScriptHashToZeroValueOutputCountMap.find(scriptSummary.codeScriptHash);
            if (foundCodeScriptToZeroValueOutputCountMapIt == codeScriptHashToZeroValueOutputCountMap.end()) {
                // If it doesn't exist, then just initialize it
                codeScriptHashToZeroValueOutputCountMap.insert(std::pair(scriptSummary.codeScriptHash, 0));
                foundCodeScriptToZeroValueOutputCountMapIt = codeScriptHashToZeroValueOutputCountMap.find(scriptSummary.codeScriptHash);
            }
            if (nValue == Amount::zero()) {
                foundCodeScriptToZeroValueOutputCountMapIt->second++;
            }
        }
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

    /// Get the transaction id hash being executed
    TxId GetTxId() const { return shared->tx.GetId(); }

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

    Amount getRefHashValueSumUtxos(const uint256& refHash) const {
        auto refHashMapIt = shared->inputRefHashToAmountMap.find(refHash);
        if (refHashMapIt != shared->inputRefHashToAmountMap.end()) {
            return refHashMapIt->second;
        }
        return Amount::zero();
    }

    Amount getRefHashValueSumOutputs(const uint256& refHash) const {
        auto refHashMapIt = shared->outputRefHashToAmountMap.find(refHash);
        if (refHashMapIt != shared->outputRefHashToAmountMap.end()) {
            return refHashMapIt->second;
        }
        return Amount::zero();
    }

    const uint256& getRefHashDataSummaryUtxo(uint32_t inputIndex) const {
        return shared->inputIndexToRefHashDataSummaryHash[inputIndex];
    }

    const uint256& getRefHashDataSummaryOutput(uint32_t outputIndex) const {
        return shared->outputIndexRefHashToDataSummaryHash[outputIndex];
    }

    uint32_t getRefTypeUtxo(const uint288& refAssetId) const {
        auto foundSingletonIt = shared->inputSingletonRefSet.find(refAssetId);
        if (foundSingletonIt != shared->inputSingletonRefSet.end()) {
            return 2; // singleton
        }
        auto foundIt = shared->inputPushRefSet.find(refAssetId);
        if (foundIt != shared->inputPushRefSet.end()) {
            return 1; // normal
        }
        return 0;
    }

    uint32_t getRefTypeOutput(const uint288& refAssetId) const {
        auto foundSingletonIt = shared->outputSingletonRefSet.find(refAssetId);
        if (foundSingletonIt != shared->outputSingletonRefSet.end()) {
            return 2; // singleton
        }
        auto foundIt = shared->outputPushRefSet.find(refAssetId);
        if (foundIt != shared->outputPushRefSet.end()) {
            return 1; // normal
        }
        return 0;
    }

    uint32_t getStateSeperatorIndexUtxo(uint32_t inputIndex) const {
        auto foundIt = shared->inputIndexToStateSeperatorIndex.find(inputIndex);
        if (foundIt != shared->inputIndexToStateSeperatorIndex.end()) {
            return foundIt->second;
        }
        return 0;
    }
    uint32_t getStateSeperatorIndexOutput(uint32_t outputIndex) const {
        auto foundIt = shared->outputIndexToStateSeperatorIndex.find(outputIndex);
        if (foundIt != shared->outputIndexToStateSeperatorIndex.end()) {
            return foundIt->second;
        }
        return 0;
    }

    Amount getRefValueSumUtxos(const uint288& refAssetId) const {
        auto foundIt = shared->inputRefAssetIdToAmountMap.find(refAssetId);
        if (foundIt != shared->inputRefAssetIdToAmountMap.end()) {
            return foundIt->second;
        }
        return Amount::zero();
    }

    Amount getRefValueSumOutputs(const uint288& refAssetId) const {
        auto foundIt = shared->outputRefAssetIdToAmountMap.find(refAssetId);
        if (foundIt != shared->outputRefAssetIdToAmountMap.end()) {
            return foundIt->second;
        }
        return Amount::zero();
    }

    uint32_t getRefOutputCountUtxos(const uint288& refAssetId) const {
        auto foundIt = shared->inputRefAssetIdToOutputCountMap.find(refAssetId);
        if (foundIt != shared->inputRefAssetIdToOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }

    uint32_t getRefOutputCountOutputs(const uint288& refAssetId) const {
        auto foundIt = shared->outputRefAssetIdToOutputCountMap.find(refAssetId);
        if (foundIt != shared->outputRefAssetIdToOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }
    uint32_t getRefOutputZeroValuedCountUtxos(const uint288& refAssetId) const {
        auto foundIt = shared->inputRefAssetIdToZeroValueOutputCountMap.find(refAssetId);
        if (foundIt != shared->inputRefAssetIdToZeroValueOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }
    uint32_t getRefOutputZeroValuedCountOutputs(const uint288& refAssetId) const {
        auto foundIt = shared->outputRefAssetIdToZeroValueOutputCountMap.find(refAssetId);
        if (foundIt != shared->outputRefAssetIdToZeroValueOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }
 
    bool getRefsPerUtxo(uint32_t inputIndex, std::vector<uint8_t>& refsVector) const {
        auto pushRefSetPerInput = shared->vectorInputsPushRefScriptSummary[inputIndex].pushRefSet;
        if (pushRefSetPerInput.size() <= 0) {
            return false;
        }
        // We know there is at least one element, therefore initialize it
        std::set<uint288>::const_iterator pushRefsIt;
        for (pushRefsIt = pushRefSetPerInput.begin(); pushRefsIt != pushRefSetPerInput.end(); pushRefsIt++) {
            std::move(pushRefsIt->begin(), pushRefsIt->end(), std::back_inserter(refsVector));
        }
        return true;
    }

    bool getRefsPerOutput(uint32_t outputIndex, std::vector<uint8_t>& refsVector) const {
        auto pushRefSetPerOutput= shared->vectorOutputsPushRefScriptSummary[outputIndex].pushRefSet;
        if (pushRefSetPerOutput.size() <= 0) {
            return false;
        }
        // We know there is at least one element, therefore initialize it
        
        std::set<uint288>::const_iterator pushRefsIt;
        for (pushRefsIt = pushRefSetPerOutput.begin(); pushRefsIt != pushRefSetPerOutput.end(); pushRefsIt++) {
            std::move(pushRefsIt->begin(), pushRefsIt->end(), std::back_inserter(refsVector));
        }
        return true;
    }
 
    Amount getCodeScriptHashValueSumUtxos(const uint256& codeScriptHash) const {
        auto foundIt = shared->inputCodeScriptHashToAmountMap.find(codeScriptHash);
        if (foundIt != shared->inputCodeScriptHashToAmountMap.end()) {
            return foundIt->second;
        }
        return Amount::zero();
    }
    Amount getCodeScriptHashValueSumOutputs(const uint256& codeScriptHash) const {
        auto foundIt = shared->outputCodeScriptHashToAmountMap.find(codeScriptHash);
        if (foundIt != shared->outputCodeScriptHashToAmountMap.end()) {
            return foundIt->second;
        }
        return Amount::zero();
    }
    uint32_t getCodeScriptHashOutputCountUtxos(const uint256& codeScriptHash) const {
        auto foundIt = shared->inputCodeScriptHashToOutputCountMap.find(codeScriptHash);
        if (foundIt != shared->inputCodeScriptHashToOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }
    uint32_t getCodeScriptHashOutputCountOutputs(const uint256& codeScriptHash) const {
        auto foundIt = shared->outputCodeScriptHashToOutputCountMap.find(codeScriptHash);
        if (foundIt != shared->outputCodeScriptHashToOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }
    uint32_t getCodeScriptHashOutputZeroValuedCountUtxos(const uint256& codeScriptHash) const {
        auto foundIt = shared->inputCodeScriptHashToZeroValueOutputCountMap.find(codeScriptHash);
        if (foundIt != shared->inputCodeScriptHashToZeroValueOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    }
    uint32_t getCodeScriptHashOutputZeroValuedCountOutputs(const uint256& codeScriptHash) const {
        auto foundIt = shared->outputCodeScriptHashToZeroValueOutputCountMap.find(codeScriptHash);
        if (foundIt != shared->outputCodeScriptHashToZeroValueOutputCountMap.end()) {
            return foundIt->second;
        }
        return 0;
    } 
    uint32_t getStateSeparatorByteIndexUtxo(uint32_t inputIndex) const {
        return shared->vectorInputsPushRefScriptSummary[inputIndex].stateSeperatorByteIndex;
    }
    uint32_t getStateSeparatorByteIndexOutput(uint32_t outputIndex) const {
        return shared->vectorOutputsPushRefScriptSummary[outputIndex].stateSeperatorByteIndex;
    }
};
#if defined(__GNUG__) && !defined(__clang__)
  #pragma GCC diagnostic pop
#endif // defined(__GNUG__) && !defined(__clang__)

using ScriptExecutionContextOpt = std::optional<ScriptExecutionContext>;

