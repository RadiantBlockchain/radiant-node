// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2017 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <banman.h>

#include <netaddress.h>
#include <ui_interface.h>
#include <util/system.h>
#include <util/time.h>

BanMan::BanMan(fs::path ban_file, const CChainParams &chainparams,
               CClientUIInterface *client_interface, int64_t default_ban_time)
    : m_client_interface(client_interface),
      m_ban_db(std::move(ban_file), chainparams),
      m_default_ban_time(default_ban_time) {
    if (m_client_interface) {
        m_client_interface->InitMessage(_("Loading banlist..."));
    }

    int64_t n_start = GetTimeMillis();
    m_is_dirty = false;
    BanTables banmap;
    if (m_ban_db.Read(banmap)) {
        // thread-safe setter
        SetBanned(banmap);
        // no need to write down, just read data
        SetBannedSetDirty(false);
        // sweep out unused entries
        SweepBanned();

        LogPrint(BCLog::NET,
                 "Loaded %d banned node ips/subnets from banlist.dat  %dms\n",
                 banmap.size(), GetTimeMillis() - n_start);
    } else {
        LogPrintf("Invalid or missing banlist.dat; recreating\n");
        // force write
        SetBannedSetDirty(true);
        DumpBanlist();
    }
}

BanMan::~BanMan() {
    DumpBanlist();
}

void BanMan::DumpBanlist() {
    // clean unused entries (if bantime has expired)
    SweepBanned();

    if (!BannedSetIsDirty()) {
        return;
    }

    int64_t n_start = GetTimeMillis();

    BanTables banmap;
    GetBanned(banmap);
    if (m_ban_db.Write(banmap)) {
        SetBannedSetDirty(false);
    }

    LogPrint(BCLog::NET,
             "Flushed %d banned node ips/subnets to banlist.dat  %dms\n",
             banmap.size(), GetTimeMillis() - n_start);
}

void BanMan::ClearBanned() {
    {
        LOCK(m_cs_banned);
        m_banned.clear();
        m_is_dirty = true;
    }
    // store banlist to disk
    DumpBanlist();
    if (m_client_interface) {
        m_client_interface->BannedListChanged();
    }
}

int BanMan::IsBannedLevel(const CNetAddr &net_addr) const {
    // Returns the most severe level of banning that applies to this address.
    // 0 - Not banned
    // 1 - Automatic misbehavior ban
    // 2 - Any other ban
    int level = 0;
    auto current_time = GetTime();
    LOCK(m_cs_banned);
    auto foundAddr = m_banned.addresses.find(net_addr);
    if (foundAddr != m_banned.addresses.end()) {
        // address-level ban hit
        if (current_time < foundAddr->second.nBanUntil) {
            if (foundAddr->second.banReason != BanReasonNodeMisbehaving) {
                return 2;
            }
            level = 1;
        }
        // continue scanning the subnet ban table for a higher level
    }
    // fall back to scanning the (smaller) subnet ban table
    for (const auto &it : m_banned.subNets) {
        const CSubNet &sub_net = it.first;
        const CBanEntry &ban_entry = it.second;

        if (current_time < ban_entry.nBanUntil && sub_net.Match(net_addr)) {
            if (ban_entry.banReason != BanReasonNodeMisbehaving) {
                return 2;
            }
            level = 1;
        }
    }
    return level;
}

bool BanMan::IsBanned(const CNetAddr &net_addr) const
{
    auto current_time = GetTime();
    LOCK(m_cs_banned);
    auto foundAddr = m_banned.addresses.find(net_addr);
    if (foundAddr != m_banned.addresses.end() && current_time < foundAddr->second.nBanUntil)
        return true;
    // fall back to scanning for subnet bans
    for (const auto &it : m_banned.subNets) {
        const CSubNet &sub_net = it.first;
        const CBanEntry &ban_entry = it.second;

        if (current_time < ban_entry.nBanUntil && sub_net.Match(net_addr)) {
            return true;
        }
    }
    return false;
}

bool BanMan::IsBanned(const CSubNet &sub_net) const
{
    if (sub_net.IsSingleIP())
        return IsBanned(sub_net.Network());
    auto current_time = GetTime();
    LOCK(m_cs_banned);
    auto it = m_banned.subNets.find(sub_net);
    if (it != m_banned.subNets.end()) {
        const CBanEntry &ban_entry = it->second;
        if (current_time < ban_entry.nBanUntil) {
            return true;
        }
    }
    return false;
}

CBanEntry BanMan::CreateBanEntry(const BanReason &ban_reason, int64_t ban_time_offset, bool since_unix_epoch) const
{
    CBanEntry ban_entry(GetTime(), ban_reason);

    int64_t normalized_ban_time_offset = ban_time_offset;
    bool normalized_since_unix_epoch = since_unix_epoch;
    if (ban_time_offset <= 0) {
        normalized_ban_time_offset = m_default_ban_time;
        normalized_since_unix_epoch = false;
    }
    ban_entry.nBanUntil = (normalized_since_unix_epoch ? 0 : GetTime()) +
                          normalized_ban_time_offset;
    return ban_entry;
}

void BanMan::Ban(const CNetAddr &net_addr, const BanReason &ban_reason,
                 int64_t ban_time_offset, bool since_unix_epoch)
{
    CBanEntry ban_entry = CreateBanEntry(ban_reason, ban_time_offset, since_unix_epoch);
    {
        LOCK(m_cs_banned);
        if (m_banned.addresses[net_addr].nBanUntil < ban_entry.nBanUntil) {
            // new entry or overwrite existing entry because ban was extended
            m_banned.addresses[net_addr] = ban_entry;
            m_is_dirty = true;
        } else {
            return;
        }
    }
    if (m_client_interface) {
        m_client_interface->BannedListChanged();
    }

    // store banlist to disk immediately if user requested ban
    if (ban_reason == BanReasonManuallyAdded) {
        DumpBanlist();
    }
}

void BanMan::Ban(const CSubNet &sub_net, const BanReason &ban_reason,
                 int64_t ban_time_offset, bool since_unix_epoch)
{
    if (sub_net.IsSingleIP()) {
        // make sure to send single-ip "subnet" bans to the right table
        Ban(sub_net.Network(), ban_reason, ban_time_offset, since_unix_epoch);
        return;
    }
    CBanEntry ban_entry = CreateBanEntry(ban_reason, ban_time_offset, since_unix_epoch);
    {
        LOCK(m_cs_banned);
        if (m_banned.subNets[sub_net].nBanUntil < ban_entry.nBanUntil) {
            // new entry or overwrite existing entry because ban was extended
            m_banned.subNets[sub_net] = ban_entry;
            m_is_dirty = true;
        } else {
            return;
        }
    }
    if (m_client_interface) {
        m_client_interface->BannedListChanged();
    }

    // store banlist to disk immediately if user requested ban
    if (ban_reason == BanReasonManuallyAdded) {
        DumpBanlist();
    }
}

bool BanMan::Unban(const CNetAddr &net_addr) {
    {
        LOCK(m_cs_banned);
        auto it = m_banned.addresses.find(net_addr);
        if (it == m_banned.addresses.end()) {
            return false;
        }
        m_banned.addresses.erase(it);
        m_is_dirty = true;
    }
    UnbanCommon();
    return true;
}

bool BanMan::Unban(const CSubNet &sub_net) {
    if (sub_net.IsSingleIP()) {
        // Qt sends us subnets only, so we detect if it's a single IP and
        // route the unban to the appropriate table
        return Unban(sub_net.Network());
    }
    {
        LOCK(m_cs_banned);
        if (m_banned.subNets.erase(sub_net) == 0) {
            return false;
        }
        m_is_dirty = true;
    }
    UnbanCommon();
    return true;
}

void BanMan::UnbanCommon()
{
    if (m_client_interface) {
        m_client_interface->BannedListChanged();
    }

    // store banlist to disk immediately
    DumpBanlist();
}

void BanMan::GetBanned(BanTables &banmap) {
    LOCK(m_cs_banned);
    // Sweep the banlist so expired bans are not returned
    SweepBanned();
    // create a thread safe copy
    banmap = m_banned;
}

void BanMan::SetBanned(const BanTables &banmap) {
    LOCK(m_cs_banned);
    m_banned = banmap;
    m_is_dirty = true;
}

void BanMan::SweepBanned() {
    const int64_t now = GetTime();
    bool notify_ui = false;
    {
        LOCK(m_cs_banned);
        {
            // sweep subnets
            auto it = m_banned.subNets.begin();
            while (it != m_banned.subNets.end()) {
                const CSubNet &sub_net = it->first;
                const CBanEntry &ban_entry = it->second;
                if (now > ban_entry.nBanUntil) {
                    LogPrint(
                        BCLog::NET,
                        "%s: Removed banned subnet from banlist.dat: %s\n",
                        __func__, sub_net.ToString());
                    it = m_banned.subNets.erase(it);
                    m_is_dirty = true;
                    notify_ui = true;
                } else {
                    ++it;
                }
            }
        }
        {
            // sweep addresses
            auto it = m_banned.addresses.begin();
            while (it != m_banned.addresses.end()) {
                const CNetAddr &addr = it->first;
                const CBanEntry &ban_entry = it->second;
                if (now > ban_entry.nBanUntil) {
                    LogPrint(
                        BCLog::NET,
                        "%s: Removed banned node ip from banlist.dat: %s\n",
                        __func__, addr.ToString());
                    it = m_banned.addresses.erase(it);
                    m_is_dirty = true;
                    notify_ui = true;
                } else {
                    ++it;
                }
            }
        }
    }
    // update UI
    if (notify_ui && m_client_interface) {
        m_client_interface->BannedListChanged();
    }
}

bool BanMan::BannedSetIsDirty() const {
    LOCK(m_cs_banned);
    return m_is_dirty;
}

void BanMan::SetBannedSetDirty(bool dirty) {
    // reuse m_banned lock for the m_is_dirty flag
    LOCK(m_cs_banned);
    m_is_dirty = dirty;
}
