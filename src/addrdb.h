// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_ADDRDB_H
#define BITCOIN_ADDRDB_H

#include <fs.h>
#include <netaddress.h>
#include <serialize.h>

#include <map>
#include <string>
#include <unordered_map>
#include <utility>

class CAddrMan;
class CDataStream;
class CChainParams;

class CBanEntry {
public:
    static const int CURRENT_VERSION = 1;
    int nVersion;
    int64_t nCreateTime;
    int64_t nBanUntil;

    CBanEntry() { SetNull(); }

    explicit CBanEntry(int64_t nCreateTimeIn) {
        SetNull();
        nCreateTime = nCreateTimeIn;
    }

    ADD_SERIALIZE_METHODS;

    template <typename Stream, typename Operation>
    inline void SerializationOp(Stream &s, Operation ser_action) {
        READWRITE(this->nVersion);
        READWRITE(nCreateTime);
        READWRITE(nBanUntil);
        uint8_t banReason = 2; //! For backward compatibility
        READWRITE(banReason);
    }

    void SetNull() {
        nVersion = CBanEntry::CURRENT_VERSION;
        nCreateTime = 0;
        nBanUntil = 0;
    }
};

// Used by the Ban Manager singleton. We maintain two ban tables:
// - An address-level ban table for fast address-based lookup in critical code
//   paths. All automatically generated bans for misbehaving nodes are on the
//   address-level and end up in this hash table.
// - A subnet-level ban table which is for manually added bans (RPC & Qt
//   console explicit bans).
//
// Note that the legacy ban table was a single std::map which presented a
// unified view of all bans, and we support this view (for Qt UI code and
// listbanned RPC) via the `toAggregatedMap()` method.
struct BanTables {
    using Addresses = std::unordered_map<CNetAddr, CBanEntry, SaltedNetAddrHasher>;
    using SubNets = std::unordered_map<CSubNet, CBanEntry, SaltedSubNetHasher>;
    using AggregatedMap = std::map<CSubNet, CBanEntry>;

    // Per-IP address level bans; this map can grow potentially large so we
    // should try and avoid linear searches on it in performance-critical paths
    Addresses addresses;
    // Subnet level bans; always manually added (usually is smaller),
    // linear searches are usually ok.
    SubNets subNets;

    // Returns a sorted, unified "view" of the ban list with all IP-level bans
    // mapped to subnets as: ipv4/32 ipv6/128. This is the format given to the
    // Qt UI and to RPC clients issuing a `listbanned` RPC command.
    AggregatedMap toAggregatedMap() const;

    void clear() { addresses.clear(); subNets.clear(); }
    size_t size() const { return addresses.size() + subNets.size(); }

    // -- Serialization / Deserialization --
    //
    using SerPair = std::pair<CSubNet, CBanEntry>;

    // Serialization to disk is done this way intentionally to support the
    // legacy format which serialized as a single std::map<CSubNet, CBanEntry>.
    template <typename Stream> void Serialize(Stream &s) const {
        WriteCompactSize(s, size());
        for (const auto &entry : subNets)
            ::Serialize(s, entry);
        for (const auto &entry : addresses) {
            const SerPair pair{
                std::piecewise_construct,
                std::forward_as_tuple(CSubNet{entry.first}),
                std::forward_as_tuple(entry.second)
            };
            ::Serialize(s, pair);
        }
    }

    // Deserialization is "as-if" this were a serialized std::map (support for
    // the legacy ban table format).
    template <typename Stream> void Unserialize(Stream &s) {
        clear();
        auto size = ReadCompactSize(s);
        while (size-- > 0) {
            SerPair entry;
            ::Unserialize(s, entry);
            // sub-nets that are basically single-ip's: ipv4/32 & ipv6/128 get
            // put in the addresses table, and real actual subnets in the
            // subNets table.
            if (entry.first.IsSingleIP())
                addresses.emplace(std::piecewise_construct,
                                  std::forward_as_tuple(entry.first.Network()),
                                  std::forward_as_tuple(entry.second));
            else
                subNets.insert(std::move(entry));
        }
    }
};


/** Access to the (IP) address database (peers.dat) */
class CAddrDB {
private:
    fs::path pathAddr;
    const CChainParams &chainParams;

public:
    CAddrDB(const CChainParams &chainParams);
    bool Write(const CAddrMan &addr);
    bool Read(CAddrMan &addr);
    bool Read(CAddrMan &addr, CDataStream &ssPeers);
};

/** Access to the banlist database (banlist.dat) */
class CBanDB {
private:
    const fs::path m_ban_list_path;
    const CChainParams &chainParams;

public:
    CBanDB(fs::path ban_list_path, const CChainParams &_chainParams);
    bool Write(const BanTables &banSet);
    bool Read(BanTables &banSet);
};

#endif // BITCOIN_ADDRDB_H
