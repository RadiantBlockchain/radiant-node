// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <chainparamsbase.h>
#include <consensus/params.h>
#include <primitives/block.h>
#include <protocol.h>

#include <array>
#include <memory>
#include <type_traits>
#include <vector>

//! Convenience class that is a CService but is aggregate-initializable.
//! See chainparamseeds.h for where it is used.
struct SeedSpec6 : public CService {
    SeedSpec6() noexcept {}

    //! Parses a human readable host:port pair to construct a valid SeedSpec6.
    //! Throws std::invalid_argument on parse failure. This is used in
    //! chainparamsseeds.h to build the internal list of seeds.
    //!
    //! Valid examples of human-reabale strings this constructor can parse:
    //!
    //!   IP4: "167.172.41.140:8333"
    //!   IP6: "[2001:470:8f9e:944:5054:ff:fed7:c164]:8333"
    //!        "[2a0a:51c0:0:136::4]:8333"
    SeedSpec6(const char *pszHostPort);
    SeedSpec6(const std::string &s) : SeedSpec6(s.c_str()) {}

    //! Convenience -- copy-construct this from a CService
    SeedSpec6(const CService &cs) : CService(cs) {}

    //! Constructor used for inline aggregate initialization
    SeedSpec6(const Span<const uint8_t> addr_, uint16_t port_) noexcept {
        static_assert(std::is_same_v<decltype(m_addr)::value_type, uint8_t>);
        port = port_;
        SetLegacyIPv6(addr_);
    }
};

typedef std::map<int, BlockHash> MapCheckpoints;

struct CCheckpointData {
    MapCheckpoints mapCheckpoints;
};

/**
 * Holds various statistics on transactions within a chain. Used to estimate
 * verification progress during chain sync.
 *
 * See also: CChainParams::TxData, GuessVerificationProgress.
 */
struct ChainTxData {
    int64_t nTime;
    int64_t nTxCount;
    double dTxRate;
};

/**
 * CChainParams defines various tweakable parameters of a given instance of the
 * Bitcoin system. There are three: the main network on which people trade goods
 * and services, the public test network which gets reset from time to time and
 * a regression test mode which is intended for private networks only. It has
 * minimal difficulty to ensure that blocks can be found instantly.
 */
class CChainParams {
public:
    enum Base58Type {
        PUBKEY_ADDRESS,
        SCRIPT_ADDRESS,
        SECRET_KEY,
        EXT_PUBLIC_KEY,
        EXT_SECRET_KEY,

        MAX_BASE58_TYPES
    };

    const Consensus::Params &GetConsensus() const { return consensus; }
    const CMessageHeader::MessageMagic &DiskMagic() const { return diskMagic; }
    const CMessageHeader::MessageMagic &NetMagic() const { return netMagic; }
    int GetDefaultPort() const { return nDefaultPort; }

    const CBlock &GenesisBlock() const { return genesis; }
    /** Default value for -checkmempool and -checkblockindex argument */
    bool DefaultConsistencyChecks() const { return fDefaultConsistencyChecks; }
    /** Policy: Filter transactions that do not match well-defined patterns */
    bool RequireStandard() const { return fRequireStandard; }
    /** If this is a test chain */
    bool IsTestChain() const { return m_is_test_chain; }
    uint64_t PruneAfterHeight() const { return nPruneAfterHeight; }
    /** Minimum free space (in GB) needed for data directory */
    uint64_t AssumedBlockchainSize() const { return m_assumed_blockchain_size; }
    /**
     * Minimum free space (in GB) needed for data directory when pruned; Does
     * not include prune target
     */
    uint64_t AssumedChainStateSize() const {
        return m_assumed_chain_state_size;
    }
    /** Whether it is possible to mine blocks on demand (no retargeting) */
    bool MineBlocksOnDemand() const { return consensus.fPowNoRetargeting; }
    /** Return the BIP70 network string (main, test or regtest) */
    std::string NetworkIDString() const { return strNetworkID; }
    /** Return the list of hostnames to look up for DNS seeds */
    const std::vector<std::string> &DNSSeeds() const { return vSeeds; }
    const std::vector<uint8_t> &Base58Prefix(Base58Type type) const {
        return base58Prefixes[type];
    }
    const std::string &CashAddrPrefix() const { return cashaddrPrefix; }
    const std::vector<SeedSpec6> &FixedSeeds() const { return vFixedSeeds; }
    const CCheckpointData &Checkpoints() const { return checkpointData; }
    const ChainTxData &TxData() const { return chainTxData; }

protected:
    CChainParams() {}

    Consensus::Params consensus;
    CMessageHeader::MessageMagic diskMagic;
    CMessageHeader::MessageMagic netMagic;
    int nDefaultPort;
    uint64_t nPruneAfterHeight;
    uint64_t m_assumed_blockchain_size;
    uint64_t m_assumed_chain_state_size;
    std::vector<std::string> vSeeds;
    std::vector<uint8_t> base58Prefixes[MAX_BASE58_TYPES];
    std::string cashaddrPrefix;
    std::string strNetworkID;
    CBlock genesis;
    std::vector<SeedSpec6> vFixedSeeds;
    bool fDefaultConsistencyChecks;
    bool fRequireStandard;
    bool m_is_test_chain;
    CCheckpointData checkpointData;
    ChainTxData chainTxData;
};

/**
 * Creates and returns a std::unique_ptr<CChainParams> of the chosen chain.
 * @returns a CChainParams* of the chosen chain.
 * @throws a std::runtime_error if the chain is not supported.
 */
std::unique_ptr<CChainParams> CreateChainParams(const std::string &chain);

CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                          int32_t nVersion, const Amount genesisReward);
CBlock CreateGenesisBlockTestnet(uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                          int32_t nVersion, const Amount genesisReward);
/**
 * Return the currently selected parameters. This won't change after app
 * startup, except for unit tests.
 */
const CChainParams &Params();

/**
 * Sets the params returned by Params() to those for the given BIP70 chain name.
 * @throws std::runtime_error when the chain is not supported.
 */
void SelectParams(const std::string &chain);
