// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_VALIDATION_H
#define BITCOIN_CONSENSUS_VALIDATION_H

#include <uint256.h>

#include <string>
#include <memory>
#include <vector>

/** "reject" message codes */
static const uint8_t REJECT_MALFORMED = 0x01;
static const uint8_t REJECT_INVALID = 0x10;
static const uint8_t REJECT_OBSOLETE = 0x11;
static const uint8_t REJECT_DUPLICATE = 0x12;
static const uint8_t REJECT_NONSTANDARD = 0x40;
static const uint8_t REJECT_INSUFFICIENTFEE = 0x42;
static const uint8_t REJECT_CHECKPOINT = 0x43;

/** Capture information about block/transaction validation */
class CValidationState {
private:
    enum mode_state {
        MODE_VALID,   //!< everything ok
        MODE_INVALID, //!< network rule violation (DoS value may be set)
        MODE_ERROR,   //!< run-time error
    } mode = MODE_VALID;
    int nDoS = 0;
    std::string strRejectReason;
    unsigned int chRejectCode = 0;
    bool corruptionPossible = false;
    std::string strDebugMessage;

    /// Validation data related to DoubleSpendProof
    struct DoubleSpend {
        /// DspId of the doublespend proof if the validation of the transaction resulted in a doublespend proof.
        /// Note: hash.IsNull() may be true, in which case validation did not result in a doublespend proof.
        uint256 hash;
        /// Validation of transaction revealed non-validating-dsproof orphans(s), sent originally from these peers.
        /// The ids in this vector may refer to peers which are no longer connected.
        /// These ids will always be peers for which HasPermission(PF_NOBAN) == false (if they are still connected).
        std::vector<int64_t> badNodeIds;
    };
    // The most common case is that *no* DSP exists. In order to minimize the memory/CPU footprint of the
    // DSProof facility, we wrap this data in a unique_ptr which will be empty in the common case.
    std::unique_ptr<DoubleSpend> dsp;
    void dspCreateIfNotExist() { if (!dsp) dsp = std::make_unique<DoubleSpend>(); }

public:
    CValidationState() = default;
    CValidationState(const CValidationState &o) { *this = o; }
    CValidationState(CValidationState &&) = default;

    /// Copy-assignment must be custom-defined due to the presense of unique_ptr (must update this if adding fields!)
    CValidationState &operator=(const CValidationState &o) {
        mode = o.mode;
        nDoS = o.nDoS;
        strRejectReason = o.strRejectReason;
        chRejectCode = o.chRejectCode;
        corruptionPossible = o.corruptionPossible;
        strDebugMessage = o.strDebugMessage;
        if (o.dsp)
            dsp = std::make_unique<DoubleSpend>(*o.dsp); // deep-copy the proof info
        else
            dsp.reset();
        return *this;
    }

    /// Default move-assignment works ok for this class.
    CValidationState &operator=(CValidationState &&) = default;

    bool DoS(int level, bool ret = false, unsigned int chRejectCodeIn = 0,
             const std::string &strRejectReasonIn = "",
             bool corruptionIn = false,
             const std::string &strDebugMessageIn = "") {
        chRejectCode = chRejectCodeIn;
        strRejectReason = strRejectReasonIn;
        corruptionPossible = corruptionIn;
        strDebugMessage = strDebugMessageIn;
        if (mode == MODE_ERROR) {
            return ret;
        }
        nDoS += level;
        mode = MODE_INVALID;
        return ret;
    }

    bool Invalid(bool ret = false, unsigned int _chRejectCode = 0,
                 const std::string &_strRejectReason = "",
                 const std::string &_strDebugMessage = "") {
        return DoS(0, ret, _chRejectCode, _strRejectReason, false,
                   _strDebugMessage);
    }
    bool Error(const std::string &strRejectReasonIn) {
        if (mode == MODE_VALID) {
            strRejectReason = strRejectReasonIn;
        }

        mode = MODE_ERROR;
        return false;
    }

    bool IsValid() const { return mode == MODE_VALID; }
    bool IsInvalid() const { return mode == MODE_INVALID; }
    bool IsError() const { return mode == MODE_ERROR; }
    bool IsInvalid(int &nDoSOut) const {
        if (IsInvalid()) {
            nDoSOut = nDoS;
            return true;
        }
        return false;
    }

    bool CorruptionPossible() const { return corruptionPossible; }
    void SetCorruptionPossible() { corruptionPossible = true; }
    unsigned int GetRejectCode() const { return chRejectCode; }
    std::string GetRejectReason() const { return strRejectReason; }
    std::string GetDebugMessage() const { return strDebugMessage; }

    // DoubleSpendProof getters and setters
    bool HasDSPHash() const { return dsp && !dsp->hash.IsNull(); }
    bool HasDSPBadNodeIds() const { return dsp && !dsp->badNodeIds.empty(); }
    uint256 GetDSPHash() const { return dsp ? dsp->hash : uint256{}; }
    std::vector<int64_t> GetDSPBadNodeIds() const { return dsp ? dsp->badNodeIds : decltype(dsp->badNodeIds){}; }
    void SetDSPHash(const uint256 &dspHash) {
        dspCreateIfNotExist();
        dsp->hash = dspHash;
    }
    void PushDSPBadNodeId(int64_t nId) {
        if (nId > -1) {
            dspCreateIfNotExist();
            dsp->badNodeIds.push_back(nId);
        }
    }
};

#endif // BITCOIN_CONSENSUS_VALIDATION_H
