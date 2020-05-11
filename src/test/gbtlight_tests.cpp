// Copyright (c) 2020 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <boost/test/unit_test.hpp>

#include <chain.h>
#include <core_io.h>
#include <gbtlight.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rpc/mining.h>
#include <streams.h>
#include <util/strencodings.h>
#include <test/setup_common.h>
#include <util/system.h>

#include <algorithm>
#include <list>
#include <set>

class GBTLightSetup : public TestingSetup
{
public:
    GBTLightSetup();
    ~GBTLightSetup();

    const int64_t argExpiryTimeSecs = 0; // disable cache expiry (no bg task) for unit testing
    const int argCacheSize = 9; // specify a custom cacheSize for testing (default is 10)
    std::vector<CTransactionRef> txs;
};


GBTLightSetup::GBTLightSetup() {
    gArgs.ForceSetArg("-gbtstoretime", strprintf("%d", argExpiryTimeSecs));
    gArgs.ForceSetArg("-gbtcachesize", strprintf("%d", argCacheSize));;
    // Initialize the gbt/ subdirectory because we will need it for this unit test.
    // (TestingSetup superclass already set up data dir)
    gbtl::Initialize(scheduler);

    // Random real transactions from mainnet and testnet; they do not to be valid on this chain for this unit test.
    std::vector<const char *> txHex = {
        "01000000016af14fe2b65b3fabe6a8f125de5ded1180aff9c3892138eefc16242f2dadfe2f00000000fd8a0100483045022100d80"
        "fa2758e4c1bc2b5b687b59d853c3a97e2b343b9ae1cb2bea0dce0e2cb1ca602200ac71e79dcde5d065ac99160be3376c8a373c016"
        "b5b6cef584b9a8eeb901b0a841483045022100d6a1a7393fa728404790bc85c26b60cf4d6d2baecfefca8b01608bb02441dc7c022"
        "056922cc8fa4d14eed39a69287a89c9d630164c23f4f810fa774e3feb6cdfea584147304402203f6a7ab7a5b91b0495ff6be292a5"
        "eee74bbf5c7b1cc6de586002ccf4142059a302200cf80778d4f4c078073d840b027a927a11d227bb87cbd043c37989f5cb01861c4"
        "14cad532102962feabd55f69c0e8eaceb7df43969dda4aeb575c7a501a4d08be392b2c48f2a2102a0e6e0def65cdb686a85c9a5cc"
        "03fc4c524831806f03cc7a18463b5267a5961421030b61fc10e70ca4fcedf95ca8284b545a5a80f255205c1c19e5eebcadbc17365"
        "921036d623ebfc46b97eb99a43d3c45df09319c8a6c9ba2b292c1a6a42e460034ed7a2103f54a07c2b5e82cf1e6465d7e37ee5a4b"
        "0701b2ccda866430190a8ebbd00f07db55aefeffffff022c1172000000000017a914e78564d75c446f8c00c757a2bd783d30c4f08"
        "19a8740e88e02000000001976a91471faafd5016aa8255d61e95cfe3c4f180504051e88ac48a80900",

        "0100000002ae54229545be8d2738e245e7ea41d089fa3def0a48e9410b49f39ec43826971d010000006a4730440220204169229eb1"
        "7dc49ad83675d693e4012453db9a8d1af6f118278152c709f6be022077081ab76df0356e53c1ba26145a3fb98ca58553a98b1c130a"
        "2f6cff4d39767f412103cfbc58232f0761a828ced4ee93e87ce27f26d005dd9c87150aad5e5f07073dcaffffffff4eca0e441d0a27"
        "f874f41739382cb80fdf3aac0f7b8316e197dd42e7155590c1010000006a47304402203832a75ccfc2f12474c1d3d2fc22cd72cc92"
        "4c1b73995a27a0d07b9c5a745f3a022035d98e1017a4cb02ff1509d17c752047dca2b270b927793f2eb9e30af1ac02d6412103cfbc"
        "58232f0761a828ced4ee93e87ce27f26d005dd9c87150aad5e5f07073dcaffffffff0260ea00000000000017a9149eefc3ae114359"
        "8a830d66cbc32aa583fa3d987687fb030100000000001976a914bddb57be877bd32264fc40670b87b6fb271813f688ac00000000",

        "0100000001993b9740d3e289876cbe6920008a35c4a08b7dc4bd48ff61b198f163af3f354900000000644102a8588b2e1a808ade29"
        "4aa76a1e63137099fa087841603a361171f0c1473396f482d8d1a61e2d3ff94280b1125114868647bff822d2a74461c6bbe6ffc06f"
        "9d412102abaad90841057ddb1ed929608b536535b0cd8a18ba0a90dba66ba7b1c1f7b4eafeffffff0176942200000000001976a91"
        "40a373caf0ab3c2b46cd05625b8d545c295b93d7a88acf3fa1400",
    };
    for (const auto & hex : txHex) {
        auto vch = ParseHex(hex);
        CDataStream stream(vch, SER_NETWORK, PROTOCOL_VERSION);
        CMutableTransaction tx;
        stream >> tx;
        txs.emplace_back(MakeTransactionRef(tx));
    }
}

GBTLightSetup::~GBTLightSetup() {
    gArgs.ClearArg("-gbtstoretime");
    gArgs.ClearArg("-gbtcachesize");
}

BOOST_FIXTURE_TEST_SUITE(gbtlight_tests, GBTLightSetup)

static bool CompareVTX(const std::vector<CTransactionRef> & a, const std::vector<CTransactionRef> & b)
{
    BOOST_CHECK(a.size() == b.size());
    for (size_t i = 0; i < a.size(); ++i) {
        bool x;
        BOOST_CHECK(x=(*a[i] == *b[i]));
        if (!x) return false;
    }
    return true;
}

/// Basic loading / saving of data to/from disk and cache. Verify what we write is what we read.
BOOST_AUTO_TEST_CASE(StoreDir_Cache_Works_Test) {
    // check that the arg worked and also that it's 0 because we may fail if it is not (theoretically)
    BOOST_REQUIRE_MESSAGE(gbtl::GetJobDataExpiry() == 0, "This test requires no background cleaner thread");

    // check saving jobId's to disk and load them and verify they work, as well as verify the in-memory cache works.
    using gbtl::JobId;
    std::list<JobId> jobs;
    std::set<JobId> jobSet;
    std::list<decltype(txs)> jobTxs;
    // generate a bunch of random job-id's (all having differing tx data)
    for (int i = 0; i < 15; ++i) {
        JobId jobId;
        int randRepeatCt = 0;
        do {
            GetRandBytes(jobId.begin(), jobId.size());
        } while (jobSet.count(jobId) && ++randRepeatCt < 5); // paranoia to ensure we generate unique job_ids each time.
        BOOST_REQUIRE_MESSAGE(randRepeatCt < 5, "Random bytes should be suitably random");
        jobTxs.emplace_back();
        auto & back = jobTxs.back();
        for (int j = 0; j < i; ++j) {
            back.insert(back.end(), txs.begin(), txs.end());
        }
        {
            LOCK(cs_main);
            // below requires cs_main
            gbtl::CacheAndSaveTxsToFile(jobId, &back);
        }
        jobs.emplace_back(jobId);
        jobSet.insert(jobId); // ensure uniqueness
        // check that the file was created where we expect
        BOOST_CHECK(fs::exists(gbtl::GetJobDataDir() / jobId.GetHex()));
        // test that loading from disk works
        CBlock fakeBlock;
        BOOST_CHECK_NO_THROW(gbtl::LoadTxsFromFile(jobId, fakeBlock));
        BOOST_CHECK(fakeBlock.vtx.size() == back.size());
        BOOST_CHECK_MESSAGE(CompareVTX(fakeBlock.vtx, back), "Loading txs from file should yield identical txs");
    }
    BOOST_REQUIRE(jobTxs.size() == jobs.size());
    BOOST_CHECK_MESSAGE(jobSet.size() == jobs.size(), "All JobIDs must be unique"); // this is a paranoia check
    // now run through again and make sure the data is either in the in-memory cache or in the disk file, just to be
    // thorough
    auto jit = jobs.rbegin();
    auto txit = jobTxs.rbegin();
    size_t found = 0;
    for ( ; jit != jobs.rend() && txit != jobTxs.rend(); ++jit, ++txit, ++found) {
        CBlock fakeBlock;
        if (!gbtl::GetTxsFromCache(*jit, fakeBlock))
            gbtl::LoadTxsFromFile(*jit, fakeBlock);
        BOOST_CHECK_MESSAGE(CompareVTX(fakeBlock.vtx, *txit),
                            "Loading txs from file or cache should yield identical txs");
    }
    BOOST_CHECK_MESSAGE(found == jobs.size(), "All should be found");
}

/// GBTLight in-memory cache tests:
/// 1. Test the cache works
/// 2. Test the cache size argument takes effect
/// 3. Test that the cache has LRU behavior
BOOST_AUTO_TEST_CASE(CacheTest) {
    using gbtl::JobId;
    std::list<JobId> jobs;
    const int cacheSize = int(gbtl::GetJobCacheSize());
    BOOST_REQUIRE(cacheSize && cacheSize == argCacheSize);
    // overfill memcache with 2x its capacity
    for (int i = 0; i < cacheSize * 2; ++i) {
        jobs.emplace_back();
        auto & jobId = jobs.back();
        GetRandBytes(jobId.begin(), jobId.size());
        {
            LOCK(cs_main);
            // below requires cs_main
            gbtl::CacheAndSaveTxsToFile(jobId, &txs);
        }
    }
    // test LRU-ness of cache
    int i = 0, okct = 0;
    for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
        CBlock block;
        const bool b = gbtl::GetTxsFromCache(*it, block);
        if (i < cacheSize) {
            BOOST_CHECK_MESSAGE(b, "Item is expected to be in the cache");
            ++okct;
            BOOST_CHECK_MESSAGE(CompareVTX(block.vtx, txs), "Loading tx's from cache should yield the expected txs");
        } else {
            BOOST_CHECK_MESSAGE(!b, "Item is not expected to be in the cache");
        }
        ++i;
    }
    BOOST_CHECK(okct == cacheSize);
}

/// Check that the merkle branch algorithm works as expected. We check both an odd number and an even number
/// of hashes to test the if (even) conditional in CheckMerkleBranch.
BOOST_AUTO_TEST_CASE(CheckMerkleBranch) {
    std::vector<std::string> hashesHexOdd = {
        "36481abad15ab97dc1cb0dc0160e76fcb156641126659827854f637fb8bc3211",
        "41e13336395c687843e290c9092f1a5653513768b930b192885d28b48e103ac6",
        "4ae59f08f366050e9214aba37c668b6923beb1aad8a47a8cd188fc529a09d35e",
        "d762ab6892fecc00f9f8bb1c5ed72a545a59e19c927f6359b16d37a8c8eb57c9",
        "e180d4fdab01ef41f574a643b4e9199511f61c6efb9c0b704257bc2d54d7c2d3",
        "eaf1aa9c4920a088317fce38c5063c2afbbf0aea6a21ab149ece8ea75370be6f",
        "efc07526f6210fb9321f9009b9ba99b47c2c1d76b20263afca293b9745762ba8",
        "fb691ae8405e5747c73300d2c0b010786d8054148a5a807fd9f6e516a7f3cc07",
        "34976152cb3962c9d57e7ae23e2e6d0bc38c1d4aa66816ad197cc20c553fbb7e",
        "fde44f08046080626c2131c5d101fb713b68b128ca8aa545dad920e394ef3547",
        "b632afc0dddf9230461c59f98d999bc360e245eaed2e8de568c443d234ef993c",
    },
    expectedHexOdd = {
        "34976152cb3962c9d57e7ae23e2e6d0bc38c1d4aa66816ad197cc20c553fbb7e",
        "2dd3e7025afb01e84eb88b563b51eaea2d736ef1a6a0478a3a14c72c58c8a3fe",
        "fb5c464a069bdfe24711a33df411a8adea8fa50fad9c9b50f777161276d44e8d",
        "9680b2aa253a148b60904fb770a7b3f3a01e547e6764778b6cc3f8dae7793dc0"
    },
    hashesHexEven = {
        "44b2a9afc29d7d65aba3849ee0be95693045169f9c3101b462c94dc3e94280bf",
        "4cba2535674655987d995d302f835a9a4c4b977d3d22be0db6f1af6cabc6d7db",
        "d762ab6892fecc00f9f8bb1c5ed72a545a59e19c927f6359b16d37a8c8eb57c9",
        "e180d4fdab01ef41f574a643b4e9199511f61c6efb9c0b704257bc2d54d7c2d3",
    },
    expectedHexEven = {
        "44b2a9afc29d7d65aba3849ee0be95693045169f9c3101b462c94dc3e94280bf",
        "8ec6e18ab42383a44baa3f29cdcfee1192fe06964a8a41a1b2917173df6fff31",
        "36d88ff7086007bf1c870fac7ccae6ab7da409dc4ab1285b1dd393c29e8bd5a8",
    };
    std::vector<uint256> hashesO, expectedO, hashesE, expectedE;
    for (const auto & h : hashesHexOdd) {
        hashesO.emplace_back();
        hashesO.back().SetHex(h);
    }
    std::sort(hashesO.begin(), hashesO.end());
    for (const auto & e : expectedHexOdd) {
        expectedO.emplace_back();
        expectedO.back().SetHex(e);
    }
    for (const auto & h : hashesHexEven) {
        hashesE.emplace_back();
        hashesE.back().SetHex(h);
    }
    std::sort(hashesE.begin(), hashesE.end());
    for (const auto & e : expectedHexEven) {
        expectedE.emplace_back();
        expectedE.back().SetHex(e);
    }

    // first, test an empty hashes vector (as can happen if a block has no tx's)
    BOOST_CHECK_MESSAGE(gbtl::MakeMerkleBranch({}).empty(),
                        "MakeMerkleBranch with an empty vector should return an empty vector");

    // next, test the case where the vector only has 1 item -- it should just return the same item again
    // (this tests a branch in the code that skips the while loop)
    BOOST_CHECK_MESSAGE(gbtl::MakeMerkleBranch({hashesO.front()}) == std::vector<uint256>{hashesO.front()},
                        "MakeMerkleBranch with a single-item vector should return the same vector");

    // lastly, test the full set
    const auto resOdd = gbtl::MakeMerkleBranch(std::move(hashesO));
    BOOST_CHECK_MESSAGE(resOdd == expectedO, "MakeMerkleBranch (odd) should yield the expected results");
    // Eveb
    const auto resEven = gbtl::MakeMerkleBranch(std::move(hashesE));
    BOOST_CHECK_MESSAGE(resEven == expectedE, "MakeMerkleBranch (even) should yield the expected results");
}

BOOST_AUTO_TEST_SUITE_END()
