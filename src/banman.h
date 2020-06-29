// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
#ifndef BITCOIN_BANMAN_H
#define BITCOIN_BANMAN_H

#include <addrdb.h>
#include <bloom.h>
#include <fs.h>
#include <sync.h>

#include <cstdint>
#include <memory>

// Default 24-hour ban on manual bans. Automatic bans for misbehavior are always
// "discouraged" until restart and/or ClearDiscouraged() is called.
// NOTE: When adjusting this, update rpcnet:setban's help ("24h")
static constexpr unsigned int DEFAULT_MANUAL_BANTIME = 60 * 60 * 24;

class CClientUIInterface;
class CNetAddr;
class CSubNet;

// Banman manages two related but distinct concepts:
//
// 1. Banning. This is configured manually by the user, through the setban RPC (or
//    Qt UI). If an address or subnet is banned, we never accept incoming connections
//    from it and never create outgoing connections to it. We won't gossip its address
//    to other peers in addr messages. Banned addresses and subnets are stored to
//    banlist.dat on shutdown and reloaded on startup. Banning can be used to prevent
//    connections with spy nodes or other griefers.
//
// 2. Discouragement. If a peer misbehaves enough (see Misbehaving() in
//    net_processing.cpp), we'll mark that address as discouraged. We still allow
//    incoming connections from them, but they're preferred for eviction when
//    we receive new incoming connections. We never make outgoing connections to
//    them, and do not gossip their address to other peers. This is implemented as
//    a bloom filter. We can (probabilistically) test for membership, but can't
//    list all discouraged addresses or unmark them as discouraged. Discouragement
//    can prevent our limited connection slots being used up by incompatible
//    or broken peers.
//
// Neither banning nor discouragement are protections against denial-of-service
// attacks, since if an attacker has a way to waste our resources and we
// disconnect from them and ban that address, it's trivial for them to
// reconnect from another IP address.
//
// Attempting to automatically disconnect or ban any class of peer carries the
// risk of splitting the network. For example, if we banned/disconnected for a
// transaction that fails a policy check and a future version changes the
// policy check so the transaction is accepted, then that transaction could
// cause the network to split between old nodes and new nodes.

class BanMan {
public:
    ~BanMan();
    BanMan(fs::path ban_file, const CChainParams &chainparams,
           CClientUIInterface *client_interface, int64_t default_ban_time);
    void Ban(const CNetAddr &net_addr, int64_t ban_time_offset = 0,
             bool since_unix_epoch = false, bool save_to_disk = true);
    void Ban(const CSubNet &sub_net, int64_t ban_time_offset = 0,
             bool since_unix_epoch = false, bool save_to_disk = true);
    void Discourage(const CNetAddr &net_addr);

    //! Clears all the ban tables (but not the discouraged set)
    void ClearBanned();
    //! Clears all discouraged addresses only
    void ClearDiscouraged();
    //! Clears both the ban tables and the discouraged set.
    void ClearAll() { ClearDiscouraged(); ClearBanned(); }

    //! Return whether net_addr is banned (complexity: constant or linear)
    //  If net_addr is banned on a per-address level, the complexity here is
    //  a constant-time lookup. If net_addr is not banned by address or is
    //  banned by subnet, then the complexity of this check is linear to the
    //  size of the subnet ban table.
    bool IsBanned(const CNetAddr &net_addr) const;

    //! Return whether sub_net is exactly banned (complexity: constant)
    bool IsBanned(const CSubNet &sub_net) const;

    //! Return whether net_addr is discouraged (complexity: constant)
    bool IsDiscouraged(const CNetAddr &net_addr) const;

    bool Unban(const CNetAddr &net_addr);
    bool Unban(const CSubNet &sub_net);
    void GetBanned(BanTables &banmap);
    void DumpBanlist();

    //! The discourage set is guaranteed to be able to store at least this many IP addresses
    static constexpr uint32_t DiscourageFilterSize() noexcept { return 50000; }
    //! The discourage set has this probability of false positives
    static constexpr double   DiscourageFalsePositiveRate() noexcept { return 0.000001; }

private:
    void SetBanned(const BanTables &banmap);
    bool BannedSetIsDirty() const;
    //! set the "dirty" flag for the banlist
    void SetBannedSetDirty(bool dirty = true);
    //! clean unused entries (if bantime has expired)
    void SweepBanned();

    CBanEntry CreateBanEntry(int64_t ban_time_offset, bool since_unix_epoch) const;
    void UnbanCommon();

    mutable RecursiveMutex m_cs_banned;
    BanTables m_banned GUARDED_BY(m_cs_banned);
    bool m_is_dirty GUARDED_BY(m_cs_banned);
    CClientUIInterface *m_client_interface = nullptr;
    CBanDB m_ban_db;
    const int64_t m_default_ban_time;
    CRollingBloomFilter m_discouraged GUARDED_BY(m_cs_banned) {DiscourageFilterSize(), DiscourageFalsePositiveRate()};
};

extern std::unique_ptr<BanMan> g_banman;

#endif // BITCOIN_BANMAN_H
