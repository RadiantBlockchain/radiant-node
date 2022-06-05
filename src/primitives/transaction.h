// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2019 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <amount.h>
#include <feerate.h>
#include <primitives/txid.h>
#include <script/script.h>
#include <serialize.h>
#include "hash.h"

static const int SERIALIZE_TRANSACTION = 0x00;

/**
 * The transaction id preimage is constituted from the parts of a transaction.
 * This is a more compact representation of the parts of a transaction. 
 * 
 * The key innovation is that we compress the inputs, prevouts, and outputs into a constant size data structure
 * This allows arbitrary induction proofs by checking the parent and grand parent transactions.
 * 
 * Make note that the 'hashOutputs' is not the same as the Sighash.Preimage, below it is the sha256 hash
 * of the concatentation of the individual sha256 hashes of each output. This is done so that a smart contract
 * can optionally verify a smaller set of data instead of requiring all complete outputs.
 * 
 */
class CTransactionHashPreimage {
public:
    int32_t nVersion;           // Tx version
    uint32_t nTotalInputs;      // Total number of inputs
    uint256 hashPrevoutInputs;  // sha256 hash of the complete inputs  
    uint256 hashSequence;       // sha256 hash of the input sequences
    uint32_t nTotalOutputs;     // Total number of outputs
    uint256 hashOutputHashes;   // sha256 hash of the concat of the sha256 hash of each output and it's pushrefs/contents
    uint32_t nLockTime;         // Tx lock time

    CTransactionHashPreimage() { SetNull(); }
 
    SERIALIZE_METHODS(CTransactionHashPreimage, obj) {
        READWRITE(
            obj.nVersion, 
            obj.nTotalInputs, 
            obj.hashPrevoutInputs, 
            obj.hashSequence, 
            obj.nTotalOutputs,
            obj.hashOutputHashes,
            obj.nLockTime
        );
    }

    void SetNull() {
        nVersion = 0;
        nTotalInputs = 0;
        hashPrevoutInputs.SetNull();
        hashSequence.SetNull();
        nTotalOutputs = 0;
        hashOutputHashes.SetNull();
        nLockTime = 0;
    }
};

/**
 * An outpoint - a combination of a transaction hash and an index n into its
 * vout.
 */
class COutPoint {
private:
    TxId txid;
    uint32_t n;

public:
    static constexpr uint32_t NULL_INDEX = std::numeric_limits<uint32_t>::max();

    COutPoint() : txid(), n(NULL_INDEX) {}
    COutPoint(TxId txidIn, uint32_t nIn) : txid(txidIn), n(nIn) {}

    SERIALIZE_METHODS(COutPoint, obj) { READWRITE(obj.txid, obj.n); }

    bool IsNull() const { return txid.IsNull() && n == NULL_INDEX; }

    const TxId &GetTxId() const { return txid; }
    uint32_t GetN() const { return n; }

    friend bool operator<(const COutPoint &a, const COutPoint &b) {
        int cmp = a.txid.Compare(b.txid);
        return cmp < 0 || (cmp == 0 && a.n < b.n);
    }

    friend bool operator==(const COutPoint &a, const COutPoint &b) {
        return (a.txid == b.txid && a.n == b.n);
    }

    friend bool operator!=(const COutPoint &a, const COutPoint &b) {
        return !(a == b);
    }

    std::string ToString() const;
};

/**
 * An input of a transaction. It contains the location of the previous
 * transaction's output that it claims and a signature that matches the output's
 * public key.
 */
class CTxIn {
public:
    COutPoint prevout;
    CScript scriptSig;
    uint32_t nSequence;

    /**
     * Setting nSequence to this value for every input in a transaction disables
     * nLockTime.
     */
    static const uint32_t SEQUENCE_FINAL = 0xffffffff;

    /* Below flags apply in the context of BIP 68*/
    /**
     * If this flag set, CTxIn::nSequence is NOT interpreted as a relative
     * lock-time.
     */
    static const uint32_t SEQUENCE_LOCKTIME_DISABLE_FLAG = (1U << 31);

    /**
     * If CTxIn::nSequence encodes a relative lock-time and this flag is set,
     * the relative lock-time has units of 512 seconds, otherwise it specifies
     * blocks with a granularity of 1.
     */
    static const uint32_t SEQUENCE_LOCKTIME_TYPE_FLAG = (1 << 22);

    /**
     * If CTxIn::nSequence encodes a relative lock-time, this mask is applied to
     * extract that lock-time from the sequence field.
     */
    static const uint32_t SEQUENCE_LOCKTIME_MASK = 0x0000ffff;

    /**
     * In order to use the same number of bits to encode roughly the same
     * wall-clock duration, and because blocks are naturally limited to occur
     * every 600s on average, the minimum granularity for time-based relative
     * lock-time is fixed at 512 seconds. Converting from CTxIn::nSequence to
     * seconds is performed by multiplying by 512 = 2^9, or equivalently
     * shifting up by 9 bits.
     */
    static const int SEQUENCE_LOCKTIME_GRANULARITY = 9;

    CTxIn() { nSequence = SEQUENCE_FINAL; }

    explicit CTxIn(COutPoint prevoutIn, CScript scriptSigIn = CScript(),
                   uint32_t nSequenceIn = SEQUENCE_FINAL)
        : prevout(prevoutIn), scriptSig(scriptSigIn), nSequence(nSequenceIn) {}
    CTxIn(TxId prevTxId, uint32_t nOut, CScript scriptSigIn = CScript(),
          uint32_t nSequenceIn = SEQUENCE_FINAL)
        : CTxIn(COutPoint(prevTxId, nOut), scriptSigIn, nSequenceIn) {}

    SERIALIZE_METHODS(CTxIn, obj) { READWRITE(obj.prevout, obj.scriptSig, obj.nSequence); }

    friend bool operator==(const CTxIn &a, const CTxIn &b) {
        return (a.prevout == b.prevout && a.scriptSig == b.scriptSig &&
                a.nSequence == b.nSequence);
    }

    friend bool operator!=(const CTxIn &a, const CTxIn &b) { return !(a == b); }

    std::string ToString() const;
};

/**
 * An output of a transaction.  It contains the public key that the next input
 * must be able to sign with to claim it.
 */
class CTxOut {
public:
    Amount nValue;
    CScript scriptPubKey;

    CTxOut() { SetNull(); }

    CTxOut(Amount nValueIn, CScript scriptPubKeyIn)
        : nValue(nValueIn), scriptPubKey(scriptPubKeyIn) {}

    SERIALIZE_METHODS(CTxOut, obj) { READWRITE(obj.nValue, obj.scriptPubKey); }

    void SetNull() {
        nValue = -SATOSHI;
        scriptPubKey.clear();
    }

    bool IsNull() const { return nValue == -SATOSHI; }

    friend bool operator==(const CTxOut &a, const CTxOut &b) {
        return (a.nValue == b.nValue && a.scriptPubKey == b.scriptPubKey);
    }

    friend bool operator!=(const CTxOut &a, const CTxOut &b) {
        return !(a == b);
    }

    std::string ToString() const;
};

class CMutableTransaction;

/**
 * Basic transaction serialization format:
 * - int32_t nVersion
 * - std::vector<CTxIn> vin
 * - std::vector<CTxOut> vout
 * - uint32_t nLockTime
 */
template <typename Stream, typename TxType>
inline void UnserializeTransaction(TxType &tx, Stream &s) {
    s >> tx.nVersion;
    tx.vin.clear();
    tx.vout.clear();
    /* Try to read the vin. In case the dummy is there, this will be read as an
     * empty vector. */
    s >> tx.vin;
    /* We read a non-empty vin. Assume a normal vout follows. */
    s >> tx.vout;
    s >> tx.nLockTime;
}

template <typename Stream, typename TxType>
inline void SerializeTransaction(const TxType &tx, Stream &s) {
    s << tx.nVersion;
    s << tx.vin;
    s << tx.vout;
    s << tx.nLockTime;
}

class CTransaction;
using CTransactionRef = std::shared_ptr<const CTransaction>;

/**
 * The basic transaction that is broadcasted on the network and contained in
 * blocks. A transaction can contain multiple inputs and outputs.
 */
class CTransaction final {
public:
    // Default transaction version.
    static const int32_t CURRENT_VERSION = 2;

    // Changing the default transaction version requires a two step process:
    // first adapting relay policy by bumping MAX_STANDARD_VERSION, and then
    // later date bumping the default CURRENT_VERSION at which point both
    // CURRENT_VERSION and MAX_STANDARD_VERSION will be equal.
    static const int32_t MAX_STANDARD_VERSION = 2;

    // The local variables are made const to prevent unintended modification
    // without updating the cached hash value. However, CTransaction is not
    // actually immutable; deserialization and assignment are implemented,
    // and bypass the constness. This is safe, as they update the entire
    // structure, including the hash.
    const std::vector<CTxIn> vin;
    const std::vector<CTxOut> vout;
    const int32_t nVersion;
    const uint32_t nLockTime;

private:
    /** Memory only. */
    const uint256 hash;

    uint256 ComputeHash() const;

    /** Construct a CTransaction that qualifies as IsNull() */
    CTransaction();

public:
    /** Default-constructed CTransaction that qualifies as IsNull() */
    static const CTransaction null;
    //! Points to null (with a no-op deleter)
    static const CTransactionRef sharedNull;

    /** Convert a CMutableTransaction into a CTransaction. */
    explicit CTransaction(const CMutableTransaction &tx);
    explicit CTransaction(CMutableTransaction &&tx);

    /**
     * We prevent copy assignment & construction to enforce use of
     * CTransactionRef, as well as prevent new code from inadvertently copying
     * around these potentially very heavy objects.
     */
    CTransaction(const CTransaction &) = delete;
    CTransaction &operator=(const CTransaction &) = delete;

    template <typename Stream> inline void Serialize(Stream &s) const {
        SerializeTransaction(*this, s);
    }

    /**
     * This deserializing constructor is provided instead of an Unserialize
     * method. Unserialize is not possible, since it would require overwriting
     * const fields.
     */
    template <typename Stream>
    CTransaction(deserialize_type, Stream &s)
        : CTransaction(CMutableTransaction(deserialize, s)) {}

    bool IsNull() const { return vin.empty() && vout.empty(); }

    const TxId GetId() const { return TxId(hash); }
    const TxHash GetHash() const { return TxHash(hash); }

    // Return sum of txouts.
    Amount GetValueOut() const;
    // GetValueIn() is a method on CCoinsViewCache, because
    // inputs must be known to compute value in.

    /**
     * Get the total transaction size in bytes.
     * @return Total transaction size in bytes
     */
    unsigned int GetTotalSize() const;

    bool IsCoinBase() const {
        return (vin.size() == 1 && vin[0].prevout.IsNull());
    }

    friend bool operator==(const CTransaction &a, const CTransaction &b) {
        return a.GetHash() == b.GetHash();
    }

    friend bool operator!=(const CTransaction &a, const CTransaction &b) {
        return !(a == b);
    }

    std::string ToString() const;
};
#if defined(__x86_64__)
static_assert(sizeof(CTransaction) == 88,
              "sizeof CTransaction is expected to be 88 bytes");
#endif

/**
 * A mutable version of CTransaction.
 */
class CMutableTransaction {
public:
    std::vector<CTxIn> vin;
    std::vector<CTxOut> vout;
    int32_t nVersion;
    uint32_t nLockTime;

    CMutableTransaction();
    explicit CMutableTransaction(const CTransaction &tx);

    template <typename Stream> inline void Serialize(Stream &s) const {
        SerializeTransaction(*this, s);
    }

    template <typename Stream> inline void Unserialize(Stream &s) {
        UnserializeTransaction(*this, s);
    }

    template <typename Stream>
    CMutableTransaction(deserialize_type, Stream &s) {
        Unserialize(s);
    }

    /**
     * Compute the id and hash of this CMutableTransaction. This is computed on
     * the fly, as opposed to GetId() and GetHash() in CTransaction, which uses
     * a cached result.
     */
    TxId GetId() const;
    TxHash GetHash() const;

    friend bool operator==(const CMutableTransaction &a,
                           const CMutableTransaction &b) {
        return a.GetHash() == b.GetHash();
    }
};
#if defined(__x86_64__)
static_assert(sizeof(CMutableTransaction) == 56,
              "sizeof CMutableTransaction is expected to be 56 bytes");
#endif

static inline CTransactionRef MakeTransactionRef() { return CTransaction::sharedNull; }

template <typename Tx>
static inline CTransactionRef MakeTransactionRef(Tx &&txIn) {
    return std::make_shared<const CTransaction>(std::forward<Tx>(txIn));
}

/** Precompute sighash midstate to avoid quadratic hashing */
struct PrecomputedTransactionData {
    uint256 hashPrevouts, hashSequence, hashOutputs, hashOutputHashes;

    PrecomputedTransactionData() = default;

    template <class T> explicit PrecomputedTransactionData(const T &tx);
};

/// A class that wraps a pointer to either a CTransaction or a
/// CMutableTransaction and presents a uniform view of the minimal
/// intersection of both classes' exposed data.
///
/// This is used by the native introspection code to make it possible for
/// mutable txs as well constant txs to be treated uniformly for the purposes
/// of the native introspection opcodes.
///
/// Contract is: The wrapped tx or mtx pointer must have a lifetime at least
///              as long as an instance of this class.
class CTransactionView {
    const CTransaction *tx{};
    const CMutableTransaction *mtx{};
public:
    CTransactionView(const CTransaction &txIn) noexcept : tx(&txIn) {}
    CTransactionView(const CMutableTransaction &mtxIn) noexcept : mtx(&mtxIn) {}

    bool isMutableTx() const noexcept { return mtx; }

    const std::vector<CTxIn> &vin() const noexcept { return mtx ? mtx->vin : tx->vin; }
    const std::vector<CTxOut> &vout() const noexcept { return mtx ? mtx->vout : tx->vout; }
    const int32_t &nVersion() const noexcept { return mtx ? mtx->nVersion : tx->nVersion; }
    const uint32_t &nLockTime() const noexcept { return mtx ? mtx->nLockTime : tx->nLockTime; }

    TxId GetId() const { return mtx ? mtx->GetId() : tx->GetId(); }
    TxHash GetHash() const { return mtx ? mtx->GetHash() : tx->GetHash(); }

    bool operator==(const CTransactionView &o) const noexcept {
        return isMutableTx() == o.isMutableTx() && (mtx ? *mtx == *o.mtx : *tx == *o.tx);
    }
    bool operator!=(const CTransactionView &o) const noexcept { return !operator==(o); }

    /// Get a pointer to the underlying constant transaction, if such a thing exists.
    /// This is used by the validation engine which is always passed a CTransaction.
    /// Returned pointer will be nullptr if this->isMutableTx()
    const CTransaction *constantTx() const { return tx; }
};

/**
 * @brief Compute the summary data for a transaction output
 * To be used in Sighash.preimage, in TxId V3 Preimage and OP codes OP_UTXODATASUMMARY
 * 
 */
struct OutputDataSummary {
    Amount nValue;
    uint256 scriptPubKeyHash;
    uint32_t totalRefs;
    uint256 refsHash;
};

static inline OutputDataSummary getOutputDataSummary(const CScript& script, const Amount& amount, const uint256& zeroRefHash) {
    OutputDataSummary outputDataSummary;
    outputDataSummary.nValue = amount;
    CHashWriter hashWriterScriptPubKey(SER_GETHASH, 0);
    hashWriterScriptPubKey << CFlatData(script);
    outputDataSummary.scriptPubKeyHash = hashWriterScriptPubKey.GetHash();
    std::set<uint288> pushRefSet;
    std::set<uint288> requireRefSet;
    std::set<uint288> disallowSiblingRefSet;
    if (!script.GetPushRefs(pushRefSet, requireRefSet, disallowSiblingRefSet)) {
        // Fatal error parsing output should never happen
        throw std::runtime_error("Error: refs-error: parsing OP_PUSHINPUTREF, OP_REQUIREINPUTREF, OP_DISALLOWPUSHINPUTREF, or OP_DISALLOWPUSHINPUTREFSIBLING. Ensure references are 36 bytes length.");
    }
    outputDataSummary.totalRefs = pushRefSet.size();
    outputDataSummary.refsHash = zeroRefHash;
    // Write the 'color' of the output by taking the sorted set of all OP_PUSHINPUTREFs values (dedup in a map)
    if (outputDataSummary.totalRefs) {
         // Get the hash of the concatentation of all of the refs in the output
        CHashWriter hashWriterScriptPubKeyColorPushRefs(SER_GETHASH, 0);
        // Then output all colors
        std::set<uint288>::const_iterator outputIt;
        for (outputIt = pushRefSet.begin(); outputIt != pushRefSet.end(); outputIt++) {
            hashWriterScriptPubKeyColorPushRefs << *outputIt;
        }
        outputDataSummary.refsHash = hashWriterScriptPubKeyColorPushRefs.GetHash();
    }
    return outputDataSummary;
}

static inline void writeOutputVector(CHashWriter& hashWriterOutputs, const CScript& script, const Amount& amount, uint256& zeroRefHash) {
    OutputDataSummary outputSummary = getOutputDataSummary(script, amount, zeroRefHash);
    hashWriterOutputs << outputSummary.nValue;
    hashWriterOutputs << outputSummary.scriptPubKeyHash;
    hashWriterOutputs << outputSummary.totalRefs;
    hashWriterOutputs << outputSummary.refsHash;
}

/**
 * @brief Get the Hash Output Hashes object
 * 
 * Generate a hash of the outputs of a transaction with the following format:
 * 
 * <output1-nValue>
 * <sha256(output1-scriptPubKey)>
 * <sha256(
 *  output1-pushRef1
 *  output1-pushRef2
 *  ...
 *  output1-pushRef3
 * )>
 * <output2-nValue>
 * <sha256(output2-scriptPubKey)>
 * <sha256(
 *  output2-pushRef1
 *  output2-pushRef2
 * )>
 * <output3-nValue>
 * <sha256(output3-scriptPubKey)>
 * <0000000000000000000000000000000000000000000000000000000000000000>
 * 
 * 
 * In the above ^^ example the first two outputs (output1 and output2) contain 3 push refs and 2 push refs respectively.
 * The sha256 is included of the concatenation of them. The push refs are sorted lexicographically (not in order)
 * In the third output, since there are no push refs in that output, a 32 byte zero NULL is included instead.
 * 
 * Then the entire datastructure above is hashed with sha256 one more time.
 * 
 * The Purpose of the construction is to provide a compressed/succint way to prove the contents of one or more outputs
 * of a transaction.
 * 
 * @tparam T 
 * @param txTo 
 * @return uint256 
 */
template <class T> static inline uint256 GetHashOutputHashes(const T &txTo) {
    CHashWriter hashWriterOutputs(SER_GETHASH, 0);
    uint256 zeroRefHash(uint256S("0000000000000000000000000000000000000000000000000000000000000000"));
    for (const auto &txout : txTo.vout) {
        writeOutputVector(hashWriterOutputs, txout.scriptPubKey, txout.nValue, zeroRefHash);
    }
    return hashWriterOutputs.GetHash();
}
