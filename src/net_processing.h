// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NET_PROCESSING_H
#define BITCOIN_NET_PROCESSING_H

#include <consensus/params.h>
#include <net.h>
#include <sync.h>
#include <validationinterface.h>

#include <map>

extern RecursiveMutex cs_main;

/**
 * Default average delay between trickled inventory transmissions in millisec.
 * Blocks and whitelisted receivers bypass this, outbound peers get half this
 * delay.
 */
static constexpr unsigned int DEFAULT_INV_BROADCAST_INTERVAL = 500;
/**
 * Maximum number of inventory items to send per transmission.
 * Limits the impact of low-fee transaction floods.
 */
static constexpr unsigned int DEFAULT_INV_BROADCAST_RATE = 7;

class Config;

/**
 * Default for -maxorphantx, maximum number of orphan transactions kept in
 * memory.
 */
static const unsigned int DEFAULT_MAX_ORPHAN_TRANSACTIONS = 100;
/**
 * Default number of orphan+recently-replaced txn to keep around for block
 * reconstruction.
 */
static const unsigned int DEFAULT_BLOCK_RECONSTRUCTION_EXTRA_TXN = 100;

/** Default for BIP61 (sending reject messages) */
static constexpr bool DEFAULT_ENABLE_BIP61 = true;

class PeerLogicValidation final : public CValidationInterface,
                                  public NetEventsInterface {
private:
    CConnman *const connman;
    BanMan *const m_banman;

    bool SendRejectsAndCheckIfShouldDiscourage(CNode *pnode, bool enable_bip61)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

public:
    PeerLogicValidation(CConnman *connman, BanMan *banman,
                        CScheduler &scheduler, bool enable_bip61);

    /**
     * Overridden from CValidationInterface.
     */
    void
    BlockConnected(const std::shared_ptr<const CBlock> &pblock,
                   const CBlockIndex *pindexConnected,
                   const std::vector<CTransactionRef> &vtxConflicted) override;
    /**
     * Overridden from CValidationInterface.
     */
    void UpdatedBlockTip(const CBlockIndex *pindexNew,
                         const CBlockIndex *pindexFork,
                         bool fInitialDownload) override;
    /**
     * Overridden from CValidationInterface.
     */
    void BlockChecked(const CBlock &block,
                      const CValidationState &state) override;
    /**
     * Overridden from CValidationInterface.
     */
    void NewPoWValidBlock(const CBlockIndex *pindex,
                          const std::shared_ptr<const CBlock> &pblock) override;

    /**
     * Initialize a peer by adding it to mapNodeState and pushing a message
     * requesting its version.
     */
    void InitializeNode(const Config &config, CNode *pnode) override;
    /**
     * Handle removal of a peer by updating various state and removing it from
     * mapNodeState.
     */
    void FinalizeNode(const Config &config, NodeId nodeid,
                      bool &fUpdateConnectionTime) override;
    /**
     * Process protocol messages received from a given node.
     */
    bool ProcessMessages(const Config &config, CNode *pfrom,
                         std::atomic<bool> &interrupt) override;
    /**
     * Send queued protocol messages to be sent to a give node.
     *
     * @param[in]   pto             The node which we are sending messages to.
     * @param[in]   interrupt       Interrupt condition for processing threads
     * @return                      True if there is more work to be done
     */
    bool SendMessages(const Config &config, CNode *pto,
                      std::atomic<bool> &interrupt) override
        EXCLUSIVE_LOCKS_REQUIRED(pto->cs_sendProcessing);

    /**
     * Consider evicting an outbound peer based on the amount of time they've
     * been behind our tip.
     */
    void ConsiderEviction(CNode *pto, int64_t time_in_seconds)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);
    /**
     * Evict extra outbound peers. If we think our tip may be stale, connect to
     * an extra outbound.
     */
    void
    CheckForStaleTipAndEvictPeers(const Consensus::Params &consensusParams);
    /**
     * If we have extra outbound peers, try to disconnect the one with the
     * oldest block announcement.
     */
    void EvictExtraOutboundPeers(int64_t time_in_seconds)
        EXCLUSIVE_LOCKS_REQUIRED(cs_main);

    /// Called when AcceptToMemoryPool creates a double-spend proof for a tx
    /// and associates it with said tx. Only ever called at most once per
    /// proof. Notifies all peers of the new dsproof inv.
    void TransactionDoubleSpent(const CTransactionRef &ptx, const DspId &dspId) override;

    /// Called when a double-spend proof turns out to be bad either because it
    /// was a rescued orphan that was bad, or because a peer sent us a bad proof.
    /// We punish the nodeid(s) in question in that case (if they are still connected).
    void BadDSProofsDetectedFromNodeIds(const std::vector<NodeId> &nodeIds) override;

private:
    //! Next time to check for stale tip
    int64_t m_stale_tip_check_time;

    /** Enable BIP61 (sending reject messages) */
    const bool m_enable_bip61;
};

struct CNodeStateStats {
    int nMisbehavior = 0;
    int nSyncHeight = -1;
    int nCommonHeight = -1;
    std::vector<int> vHeightInFlight;
};

/** Get statistics from node state */
bool GetNodeStateStats(NodeId nodeid, CNodeStateStats &stats);
/** Increase a node's misbehavior score. */
void Misbehaving(NodeId nodeid, int howmuch, const std::string &reason = "");

// `internal` namespace exposed *FOR TESTS ONLY*
// This namespace is for exposed internals not intended for public usage.
// We would ideally have made these private to the net_processing.cpp
// translation unit only, but since some tests need to see these functions
// (see denialofservice_tests.cpp), we do this instead.
namespace internal {
struct COrphanTx {
    CTransactionRef tx;
    NodeId fromPeer;
    int64_t nTimeExpire;
};

extern RecursiveMutex g_cs_orphans;
extern std::map<TxId, COrphanTx> mapOrphanTransactions GUARDED_BY(g_cs_orphans);

bool AddOrphanTx(const CTransactionRef &tx, NodeId peer);
void EraseOrphansFor(NodeId peer);
unsigned int LimitOrphanTxSize(unsigned int nMaxOrphans);
void UpdateLastBlockAnnounceTime(NodeId node, int64_t time_in_seconds);
} // namespace internal

#endif // BITCOIN_NET_PROCESSING_H
