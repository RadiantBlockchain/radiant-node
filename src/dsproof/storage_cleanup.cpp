// Copyright (C) 2019-2020 Tom Zander <tomz@freedommail.ch>
// Copyright (C) 2020 Calin Culianu <calin.culianu@gmail.com>
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <dsproof/storage.h>
#include <logging.h>
#include <net_processing.h>

bool DoubleSpendProofStorage::periodicCleanup()
{
    std::vector<NodeId> punishPeers;
    {
        LOCK(m_lock);
        const auto expire = GetTime() - m_secondsToKeepOrphans;
        auto &index = m_proofs.get<tag_TimeStamp>();
        const auto end = index.upper_bound(expire);
        size_t erased = 0;
        for (auto it = index.begin(); it != end; ) {
            if (it->orphan) {
                if (it->nodeId > -1)
                    punishPeers.push_back(it->nodeId);
                it = index.erase(it);
                decrementOrphans(1);
                ++erased;
            } else
                ++it;
        }
        if (erased)
            LogPrint(BCLog::DSPROOF, "DSP orphans erased: %d, DSProof count: %d\n", erased, m_proofs.size());
    }
    if (!punishPeers.empty()) {
        // mark peers as misbehaving here with m_lock not held
        LOCK(cs_main);
        for (auto peerId : punishPeers)
            Misbehaving(peerId, 1, "dsproof-orphan-expired");
    }

    return true; // repeat
}
