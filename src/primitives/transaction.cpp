// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <primitives/transaction.h>

#include <hash.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <script/interpreter.h>
#include <iostream>
#include <boost/algorithm/hex.hpp>
// Hash functions copied from interpreter.cpp

/**
 * @brief Get the Inputs Hash object
 * 
 * The input hash is like the classic input hash except the sha256 of the unlocking script in the input is 
 * also included to be able to know what was in the unlocking script.
 * 
 * It allows the ability to compress it to a fixed size proof.
 * 
 * @tparam T 
 * @param txTo 
 * @return uint256 
 */
template <class T> uint256 GetHashPrevoutInputs(const T &txTo) {
    CHashWriter hashWriterInputs(SER_GETHASH, 0);
    for (const auto &txin : txTo.vin) {
        CHashWriter scriptSigHashWriter(SER_GETHASH, 0);
        hashWriterInputs << txin.prevout;
        scriptSigHashWriter << CFlatData(txin.scriptSig);
        auto scriptSigHash = scriptSigHashWriter.GetHash();;
        hashWriterInputs << scriptSigHash;
    }
    return hashWriterInputs.GetHash();
}

template <class T> uint256 GetSequenceHash(const T &txTo) {
    CHashWriter ss(SER_GETHASH, 0);
    for (const auto &txin : txTo.vin) {
        ss << txin.nSequence;
    }
    return ss.GetHash();
}

/**
 * @brief Get the Transaction Hash Preimage object
 * 
 * This is the Tx.nVersion == 3 (TxId v3) formated preimage to allow compact induction proofs.
 * 
 * @tparam T 
 * @param txTo 
 * @return CTransactionHashPreimage 
 */
template <class T> CTransactionHashPreimage GetTransactionHashPreimage(const T &txTo) {
    uint256 hashPrevoutInputs = GetHashPrevoutInputs(txTo);  
    uint256 hashSequence = GetSequenceHash(txTo);                   
    uint256 hashOutputHashes = GetHashOutputHashes(txTo);             
    CTransactionHashPreimage txIdPreimage;
    txIdPreimage.nVersion = txTo.nVersion;
    txIdPreimage.nTotalInputs = txTo.vin.size();
    txIdPreimage.hashPrevoutInputs = hashPrevoutInputs;
    txIdPreimage.hashSequence = hashSequence;
    txIdPreimage.nTotalOutputs = txTo.vout.size();
    txIdPreimage.hashOutputHashes = hashOutputHashes;
    txIdPreimage.nLockTime = txTo.nLockTime;
    return txIdPreimage;
}

std::string COutPoint::ToString() const {
    return strprintf("COutPoint(%s, %u)", txid.ToString().substr(0, 10), n);
}

std::string CTxIn::ToString() const {
    std::string str;
    str += "CTxIn(";
    str += prevout.ToString();
    if (prevout.IsNull()) {
        str += strprintf(", coinbase %s", HexStr(scriptSig));
    } else {
        str += strprintf(", scriptSig=%s", HexStr(scriptSig).substr(0, 24));
    }
    if (nSequence != SEQUENCE_FINAL) {
        str += strprintf(", nSequence=%u", nSequence);
    }
    str += ")";
    return str;
}

std::string CTxOut::ToString() const {
    return strprintf("CTxOut(nValue=%d.%08d, scriptPubKey=%s)", nValue / COIN,
                     (nValue % COIN) / SATOSHI,
                     HexStr(scriptPubKey).substr(0, 30));
}

CMutableTransaction::CMutableTransaction()
    : nVersion(CTransaction::CURRENT_VERSION), nLockTime(0) {}
CMutableTransaction::CMutableTransaction(const CTransaction &tx)
    : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion),
      nLockTime(tx.nLockTime) {}

static uint256 ComputeCMutableTransactionHash(const CMutableTransaction &tx) {
    if (tx.nVersion == 3) {
        return SerializeHash(GetTransactionHashPreimage(tx), SER_GETHASH, 0);
    }
    return SerializeHash(tx, SER_GETHASH, 0);
}

TxId CMutableTransaction::GetId() const {
    return TxId(ComputeCMutableTransactionHash(*this));
}

TxHash CMutableTransaction::GetHash() const {
    return TxHash(ComputeCMutableTransactionHash(*this));
}

uint256 CTransaction::ComputeHash() const {
    if (this->nVersion == 3) {
        auto preimage = GetTransactionHashPreimage(*this);
        std::stringstream ss;
        ::Serialize(ss, preimage);
        std::string rawByteStr = ss.str();
        std::cerr << "txid v3 preimage: " << boost::algorithm::hex(rawByteStr) << std::endl; 
        auto txHash = SerializeHash(preimage, SER_GETHASH, 0);
        std::cerr << "txid v3: " << txHash.GetHex() << std::endl;
        return txHash;
    }
    return SerializeHash(*this, SER_GETHASH, 0);
}

/*static*/ const CTransaction CTransaction::null;

//! This sharedNull is a singleton returned by MakeTransactionRef() (no args).
//! It is a 'fake' shared pointer that points to `null` above, and its deleter
//! is a no-op.
/*static*/ const CTransactionRef CTransaction::sharedNull{&CTransaction::null, [](const CTransaction *){}};

/* private - for constructing the above null value only */
CTransaction::CTransaction() : nVersion{CTransaction::CURRENT_VERSION}, nLockTime{0} {}

/* public */
CTransaction::CTransaction(const CMutableTransaction &tx)
    : vin(tx.vin), vout(tx.vout), nVersion(tx.nVersion),
      nLockTime(tx.nLockTime), hash(ComputeHash()) {}
CTransaction::CTransaction(CMutableTransaction &&tx)
    : vin(std::move(tx.vin)), vout(std::move(tx.vout)), nVersion(tx.nVersion),
      nLockTime(tx.nLockTime), hash(ComputeHash()) {}

Amount CTransaction::GetValueOut() const {
    Amount nValueOut = Amount::zero();
    for (const auto &tx_out : vout) {
        nValueOut += tx_out.nValue;
        if (!MoneyRange(tx_out.nValue) || !MoneyRange(nValueOut)) {
            throw std::runtime_error(std::string(__func__) +
                                     ": value out of range");
        }
    }
    return nValueOut;
}

unsigned int CTransaction::GetTotalSize() const {
    return ::GetSerializeSize(*this, PROTOCOL_VERSION);
}

std::string CTransaction::ToString() const {
    std::string str;
    str += strprintf("CTransaction(txid=%s, ver=%d, vin.size=%u, vout.size=%u, "
                     "nLockTime=%u)\n",
                     GetId().ToString().substr(0, 10), nVersion, vin.size(),
                     vout.size(), nLockTime);
    for (const auto &nVin : vin) {
        str += "    " + nVin.ToString() + "\n";
    }
    for (const auto &nVout : vout) {
        str += "    " + nVout.ToString() + "\n";
    }
    return str;
}
