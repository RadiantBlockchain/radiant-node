// Copyright (c) 2011-2016 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Unit tests for denial-of-service detection/prevention code

#include <banman.h>
#include <chain.h>
#include <chainparams.h>
#include <config.h>
#include <crypto/siphash.h>
#include <keystore.h>
#include <net.h>
#include <net_processing.h>
#include <pow.h>
#include <script/sign.h>
#include <serialize.h>
#include <util/system.h>
#include <validation.h>

#include <test/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <cstring>

struct CConnmanTest : public CConnman {
    using CConnman::CConnman;
    void AddNode(CNode &node) {
        LOCK(cs_vNodes);
        vNodes.push_back(&node);
    }
    void ClearNodes() {
        LOCK(cs_vNodes);
        for (CNode *node : vNodes) {
            delete node;
        }
        vNodes.clear();
    }
};

static CService ip(uint32_t i) {
    struct in_addr s;
    s.s_addr = i;
    return CService(CNetAddr(s), Params().GetDefaultPort());
}

static NodeId id = 0;

BOOST_FIXTURE_TEST_SUITE(denialofservice_tests, TestingSetup)

// Test eviction of an outbound peer whose chain never advances
// Mock a node connection, and use mocktime to simulate a peer which never sends
// any headers messages. PeerLogic should decide to evict that outbound peer,
// after the appropriate timeouts.
// Note that we protect 4 outbound nodes from being subject to this logic; this
// test takes advantage of that protection only being applied to nodes which
// send headers with sufficient work.
BOOST_AUTO_TEST_CASE(outbound_slow_chain_eviction) {
    const Config &config = GetConfig();
    std::atomic<bool> interruptDummy(false);

    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    auto peerLogic = std::make_unique<PeerLogicValidation>(
        connman.get(), nullptr, scheduler, false);

    // Mock an outbound peer
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, ServiceFlags(NODE_NETWORK), 0, INVALID_SOCKET, addr1,
                     0, 0, CAddress(), "",
                     /*fInboundIn=*/false);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);

    peerLogic->InitializeNode(config, &dummyNode1);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;

    // This test requires that we have a chain with non-zero work.
    {
        LOCK(cs_main);
        BOOST_CHECK(::ChainActive().Tip() != nullptr);
        BOOST_CHECK(::ChainActive().Tip()->nChainWork > 0);
    }

    // Test starts here
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        // should result in getheaders
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    {
        LOCK2(cs_main, dummyNode1.cs_vSend);
        BOOST_CHECK(dummyNode1.vSendMsg.size() > 0);
        dummyNode1.vSendMsg.clear();
    }

    int64_t nStartTime = GetTime();
    // Wait 21 minutes
    SetMockTime(nStartTime + 21 * 60);
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        // should result in getheaders
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    {
        LOCK2(cs_main, dummyNode1.cs_vSend);
        BOOST_CHECK(dummyNode1.vSendMsg.size() > 0);
    }
    // Wait 3 more minutes
    SetMockTime(nStartTime + 24 * 60);
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        // should result in disconnect
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    BOOST_CHECK(dummyNode1.fDisconnect == true);
    SetMockTime(0);

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode1.GetId(), dummy);
}

static void AddRandomOutboundPeer(const Config &config,
                                  std::vector<CNode *> &vNodes,
                                  PeerLogicValidation &peerLogic,
                                  CConnmanTest *connman) {
    CAddress addr(ip(g_insecure_rand_ctx.randbits(32)), NODE_NONE);
    vNodes.emplace_back(new CNode(id++, ServiceFlags(NODE_NETWORK), 0,
                                  INVALID_SOCKET, addr, 0, 0, CAddress(), "",
                                  /*fInboundIn=*/false));
    CNode &node = *vNodes.back();
    node.SetSendVersion(PROTOCOL_VERSION);

    peerLogic.InitializeNode(config, &node);
    node.nVersion = 1;
    node.fSuccessfullyConnected = true;

    connman->AddNode(node);
}

BOOST_AUTO_TEST_CASE(stale_tip_peer_management) {
    const Config &config = GetConfig();

    auto connman = std::make_unique<CConnmanTest>(config, 0x1337, 0x1337);
    auto peerLogic = std::make_unique<PeerLogicValidation>(
        connman.get(), nullptr, scheduler, false);

    const Consensus::Params &consensusParams =
        config.GetChainParams().GetConsensus();
    constexpr int nMaxOutbound = 8;
    CConnman::Options options;
    options.nMaxConnections = 125;
    options.nMaxOutbound = nMaxOutbound;
    options.nMaxFeeler = 1;

    connman->Init(options);
    std::vector<CNode *> vNodes;

    // Mock some outbound peers
    for (int i = 0; i < nMaxOutbound; ++i) {
        AddRandomOutboundPeer(config, vNodes, *peerLogic, connman.get());
    }

    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);

    // No nodes should be marked for disconnection while we have no extra peers
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    SetMockTime(GetTime() + 3 * consensusParams.nPowTargetSpacing + 1);

    // Now tip should definitely be stale, and we should look for an extra
    // outbound peer
    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);
    BOOST_CHECK(connman->GetTryNewOutboundPeer());

    // Still no peers should be marked for disconnection
    for (const CNode *node : vNodes) {
        BOOST_CHECK(node->fDisconnect == false);
    }

    // If we add one more peer, something should get marked for eviction
    // on the next check (since we're mocking the time to be in the future, the
    // required time connected check should be satisfied).
    AddRandomOutboundPeer(config, vNodes, *peerLogic, connman.get());

    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);
    for (int i = 0; i < nMaxOutbound; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
    // Last added node should get marked for eviction
    BOOST_CHECK(vNodes.back()->fDisconnect == true);

    vNodes.back()->fDisconnect = false;

    // Update the last announced block time for the last
    // peer, and check that the next newest node gets evicted.
    internal::UpdateLastBlockAnnounceTime(vNodes.back()->GetId(), GetTime());

    peerLogic->CheckForStaleTipAndEvictPeers(consensusParams);
    for (int i = 0; i < nMaxOutbound - 1; ++i) {
        BOOST_CHECK(vNodes[i]->fDisconnect == false);
    }
    BOOST_CHECK(vNodes[nMaxOutbound - 1]->fDisconnect == true);
    BOOST_CHECK(vNodes.back()->fDisconnect == false);

    bool dummy;
    for (const CNode *node : vNodes) {
        peerLogic->FinalizeNode(config, node->GetId(), dummy);
    }

    connman->ClearNodes();
}

BOOST_AUTO_TEST_CASE(DoS_autodiscourage) {
    const Config &config = GetConfig();
    std::atomic<bool> interruptDummy(false);

    auto banman = std::make_unique<BanMan>(GetDataDir() / "banlist.dat",
                                           config.GetChainParams(), nullptr,
                                           DEFAULT_MANUAL_BANTIME);
    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    auto peerLogic = std::make_unique<PeerLogicValidation>(
        connman.get(), banman.get(), scheduler, false);

    banman->ClearAll();
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr1, 0, 0,
                     CAddress(), "", true);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode1);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;
    {
        LOCK(cs_main);
        // Should get discouraged.
        Misbehaving(dummyNode1.GetId(), 100, "");
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    // check discouraged but not banned
    BOOST_CHECK(banman->IsDiscouraged(addr1));
    BOOST_CHECK(!banman->IsBanned(addr1));
    // Different IP, not discouraged.
    BOOST_CHECK(!banman->IsDiscouraged(ip(0xa0b0c001 | 0x0000ff00)));

    CAddress addr2(ip(0xa0b0c002), NODE_NONE);
    CNode dummyNode2(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr2, 1, 1,
                     CAddress(), "", true);
    dummyNode2.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode2);
    dummyNode2.nVersion = 1;
    dummyNode2.fSuccessfullyConnected = true;
    {
        LOCK(cs_main);
        Misbehaving(dummyNode2.GetId(), 50, "");
    }
    {
        LOCK2(cs_main, dummyNode2.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode2, interruptDummy);
    }
    // 2 not discouraged yet...
    BOOST_CHECK(!banman->IsDiscouraged(addr2));
    // ... but 1 still should be.
    BOOST_CHECK(banman->IsDiscouraged(addr1));
    {
        LOCK(cs_main);
        Misbehaving(dummyNode2.GetId(), 50, "");
    }
    {
        LOCK2(cs_main, dummyNode2.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode2, interruptDummy);
    }
    BOOST_CHECK(banman->IsDiscouraged(addr2));
    BOOST_CHECK(!banman->IsBanned(addr2));

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode1.GetId(), dummy);
    peerLogic->FinalizeNode(config, dummyNode2.GetId(), dummy);
}

BOOST_AUTO_TEST_CASE(DoS_banscore) {
    const Config &config = GetConfig();
    std::atomic<bool> interruptDummy(false);

    auto banman = std::make_unique<BanMan>(GetDataDir() / "banlist.dat",
                                           config.GetChainParams(), nullptr,
                                           DEFAULT_MANUAL_BANTIME);
    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    auto peerLogic = std::make_unique<PeerLogicValidation>(
        connman.get(), banman.get(), scheduler, false);

    banman->ClearAll();
    // because 11 is my favorite number.
    gArgs.ForceSetArg("-banscore", "111");
    CAddress addr1(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode1(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr1, 3, 1,
                     CAddress(), "", true);
    dummyNode1.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode1);
    dummyNode1.nVersion = 1;
    dummyNode1.fSuccessfullyConnected = true;
    {
        LOCK(cs_main);
        Misbehaving(dummyNode1.GetId(), 100, "");
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    BOOST_CHECK(!banman->IsDiscouraged(addr1));
    {
        LOCK(cs_main);
        Misbehaving(dummyNode1.GetId(), 10, "");
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    BOOST_CHECK(!banman->IsDiscouraged(addr1));
    {
        LOCK(cs_main);
        Misbehaving(dummyNode1.GetId(), 1, "");
    }
    {
        LOCK2(cs_main, dummyNode1.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode1, interruptDummy);
    }
    BOOST_CHECK(banman->IsDiscouraged(addr1));
    gArgs.ForceSetArg("-banscore", std::to_string(DEFAULT_BANSCORE_THRESHOLD));

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode1.GetId(), dummy);
}

BOOST_AUTO_TEST_CASE(DoS_bantime) {
    const Config &config = GetConfig();
    std::atomic<bool> interruptDummy(false);

    auto banman = std::make_unique<BanMan>(GetDataDir() / "banlist.dat",
                                           config.GetChainParams(), nullptr,
                                           DEFAULT_MANUAL_BANTIME);
    auto connman = std::make_unique<CConnman>(config, 0x1337, 0x1337);
    auto peerLogic = std::make_unique<PeerLogicValidation>(
        connman.get(), banman.get(), scheduler, false);

    banman->ClearAll();
    int64_t nStartTime = GetTime();
    // Overrides future calls to GetTime()
    SetMockTime(nStartTime);

    CAddress addr(ip(0xa0b0c001), NODE_NONE);
    CNode dummyNode(id++, NODE_NETWORK, 0, INVALID_SOCKET, addr, 4, 4,
                    CAddress(), "", true);
    dummyNode.SetSendVersion(PROTOCOL_VERSION);
    peerLogic->InitializeNode(config, &dummyNode);
    dummyNode.nVersion = 1;
    dummyNode.fSuccessfullyConnected = true;

    {
        LOCK(cs_main);
        Misbehaving(dummyNode.GetId(), 100, "");
    }
    {
        LOCK2(cs_main, dummyNode.cs_sendProcessing);
        peerLogic->SendMessages(config, &dummyNode, interruptDummy);
    }
    const CNetAddr bannedAddr{ip(0xd0e0f002)};
    banman->Ban(bannedAddr);

    BOOST_CHECK(banman->IsBanned(bannedAddr));
    BOOST_CHECK(banman->IsDiscouraged(addr));
    BOOST_CHECK(!banman->IsBanned(addr));

    SetMockTime(nStartTime + 60 * 60);
    BOOST_CHECK(banman->IsDiscouraged(addr));
    BOOST_CHECK(banman->IsBanned(bannedAddr));

    SetMockTime(nStartTime + 60 * 60 * 24 + 1);
    // banning should expire
    BOOST_CHECK(!banman->IsBanned(bannedAddr));
    // discouragement never expires ...
    BOOST_CHECK(banman->IsDiscouraged(addr));
    // ... unless we clear the discourage bloom filter
    banman->ClearDiscouraged();
    BOOST_CHECK(!banman->IsDiscouraged(addr));

    bool dummy;
    peerLogic->FinalizeNode(config, dummyNode.GetId(), dummy);
}

static CNetAddr GenAddress() {
    // deterministic hasher to always generate the same set of addrs
    static CSipHasher hasher{0xfeedbeeff00d1234, 0x9001231231471231};
    static uint64_t last{0x123456789abcdef7};
    uint64_t lo, hi;
    last = lo = hasher.Write(last).Finalize();
    last = hi = hasher.Write(last).Finalize();
    static const auto ip6 = [](uint64_t w1, uint64_t w2) {
        struct in6_addr s;
        static_assert(sizeof(s) == sizeof(uint64_t) * 2, "ipv6 address should be 128 bits");
        uint8_t *buf = reinterpret_cast<uint8_t *>(&s);
        std::memcpy(buf, &w1, sizeof(w1));
        std::memcpy(buf + sizeof(w1), &w2, sizeof(w2));
        return CNetAddr{s};
    };
    return ip6(lo, hi);
}

BOOST_AUTO_TEST_CASE(DoS_DiscourageRolling) {
    const Config &config = GetConfig();
    auto banman = std::make_unique<BanMan>(GetDataDir() / "banlist.dat",
                                           config.GetChainParams(), nullptr,
                                           DEFAULT_MANUAL_BANTIME);
    banman->ClearAll();

    constexpr int FilterSize = int(BanMan::DiscourageFilterSize());
    constexpr int NGen = FilterSize * 1.5 + 1;
    std::vector<CNetAddr> addrs;
    addrs.reserve(NGen);
    for (int i = 0; i < NGen; ++i) {
        auto addr = GenAddress();
        banman->Discourage(addr);
        // make sure discouragement at least immediately works
        BOOST_CHECK(banman->IsDiscouraged(addr));
        addrs.push_back(addr);
    }

    // check the rolling properly of the filter in that it "forgets" older discouraged addresses after
    // its filter size is exhausted.
    BOOST_CHECK(!banman->IsDiscouraged(addrs.front()));

    // check that the most recent additions are still all discouraged (but not banned)
    int i = 0;
    for (auto rit = addrs.rbegin(); rit != addrs.rend() && i < FilterSize; ++i, ++rit) {
        BOOST_CHECK(banman->IsDiscouraged(*rit));
        BOOST_CHECK(!banman->IsBanned(*rit));
    }

    // next, clear the filter and check nothing is Discouraged anymore
    banman->ClearDiscouraged();
    for (const auto &addr : addrs) {
        BOOST_CHECK(!banman->IsDiscouraged(addr));
    }
}

static CTransactionRef RandomOrphan() {
    LOCK2(cs_main, internal::g_cs_orphans);
    auto it = internal::mapOrphanTransactions.lower_bound(TxId{InsecureRand256()});
    if (it == internal::mapOrphanTransactions.end()) {
        it = internal::mapOrphanTransactions.begin();
    }
    return it->second.tx;
}

BOOST_AUTO_TEST_CASE(DoS_mapOrphans) {
    CKey key;
    key.MakeNewKey(true);
    CBasicKeyStore keystore;
    keystore.AddKey(key);

    // 50 orphan transactions:
    for (int i = 0; i < 50; i++) {
        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout = COutPoint(TxId(InsecureRand256()), 0);
        tx.vin[0].scriptSig << OP_1;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey =
            GetScriptForDestination(key.GetPubKey().GetID());

        internal::AddOrphanTx(MakeTransactionRef(tx), i);
    }

    auto const null_context = std::nullopt; //It is Ok to have a null context here.
    // ... and 50 that depend on other orphans:
    for (int i = 0; i < 50; i++) {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vin.resize(1);
        tx.vin[0].prevout = COutPoint(txPrev->GetId(), 0);
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey =
            GetScriptForDestination(key.GetPubKey().GetID());
        SignSignature(keystore, *txPrev, tx, 0, SigHashType(), null_context);

        internal::AddOrphanTx(MakeTransactionRef(tx), i);
    }

    // This really-big orphan should be ignored:
    for (int i = 0; i < 10; i++) {
        CTransactionRef txPrev = RandomOrphan();

        CMutableTransaction tx;
        tx.vout.resize(1);
        tx.vout[0].nValue = 1 * CENT;
        tx.vout[0].scriptPubKey =
            GetScriptForDestination(key.GetPubKey().GetID());
        tx.vin.resize(2777);
        for (size_t j = 0; j < tx.vin.size(); j++) {
            tx.vin[j].prevout = COutPoint(txPrev->GetId(), j);
        }
        SignSignature(keystore, *txPrev, tx, 0, SigHashType(), null_context);
        // Re-use same signature for other inputs
        // (they don't have to be valid for this test)
        for (unsigned int j = 1; j < tx.vin.size(); j++) {
            tx.vin[j].scriptSig = tx.vin[0].scriptSig;
        }

        BOOST_CHECK(!internal::AddOrphanTx(MakeTransactionRef(tx), i));
    }

    LOCK2(cs_main, internal::g_cs_orphans);
    // Test EraseOrphansFor:
    for (NodeId i = 0; i < 3; i++) {
        size_t sizeBefore = internal::mapOrphanTransactions.size();
        internal::EraseOrphansFor(i);
        BOOST_CHECK(internal::mapOrphanTransactions.size() < sizeBefore);
    }

    // Test LimitOrphanTxSize() function:
    internal::LimitOrphanTxSize(40);
    BOOST_CHECK(internal::mapOrphanTransactions.size() <= 40);
    internal::LimitOrphanTxSize(10);
    BOOST_CHECK(internal::mapOrphanTransactions.size() <= 10);
    internal::LimitOrphanTxSize(0);
    BOOST_CHECK(internal::mapOrphanTransactions.empty());
}

BOOST_AUTO_TEST_SUITE_END()
