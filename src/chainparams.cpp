// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2016 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#include <chainparamsconstants.h>
#include <chainparamsseeds.h>
#include <consensus/consensus.h>
#include <consensus/merkle.h>
#include <netbase.h>
#include <tinyformat.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <stdexcept>

static CBlock CreateGenesisBlock(const char *pszTimestamp,
                                 const CScript &genesisOutputScript,
                                 uint32_t nTime, uint32_t nNonce,
                                 uint32_t nBits, int32_t nVersion,
                                 const Amount genesisReward) {
    CMutableTransaction txNew;
    txNew.nVersion = 1;
    txNew.vin.resize(1);
    txNew.vout.resize(1);
    txNew.vin[0].scriptSig =
        CScript() << ScriptInt::fromIntUnchecked(486604799)
                  << CScriptNum::fromIntUnchecked(4)
                  << std::vector<uint8_t>((const uint8_t *)pszTimestamp,
                                          (const uint8_t *)pszTimestamp + strlen(pszTimestamp));
    txNew.vout[0].nValue = genesisReward;
    txNew.vout[0].scriptPubKey = genesisOutputScript;

    CBlock genesis;
    genesis.nTime = nTime;
    genesis.nBits = nBits;
    genesis.nNonce = nNonce;
    genesis.nVersion = nVersion;
    genesis.vtx.push_back(MakeTransactionRef(std::move(txNew)));
    genesis.hashPrevBlock.SetNull();
    genesis.hashMerkleRoot = BlockMerkleRoot(genesis);
    return genesis;
}

/**
 * Build the genesis block. Note that the output of its generation transaction
 * cannot be spent since it did not originally exist in the database.
 *
 * CBlock(hash=000000000019d6, ver=1, hashPrevBlock=00000000000000,
 * hashMerkleRoot=4a5e1e, nTime=1231006505, nBits=1d00ffff, nNonce=2083236893,
 * vtx=1)
 *   CTransaction(hash=4a5e1e, ver=1, vin.size=1, vout.size=1, nLockTime=0)
 *     CTxIn(COutPoint(000000, -1), coinbase
 * 04ffff001d0104455468652054696d65732030332f4a616e2f32303039204368616e63656c6c6f72206f6e206272696e6b206f66207365636f6e64206261696c6f757420666f722062616e6b73)
 *     CTxOut(nValue=50.00000000, scriptPubKey=0x5F1DF16B2B704C8A578D0B)
 *   vMerkleTree: 4a5e1e
 */
CBlock CreateGenesisBlock(uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                          int32_t nVersion, const Amount genesisReward) {
    const char *pszTimestamp =
        "The New York Times 12/May/2022 The Milky Way's Black Hole Comes to Light";
    const CScript genesisOutputScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("4f9ab47a237441c0dc61dcd39fd92709f281b033") << OP_EQUALVERIFY << OP_CHECKSIG;
        //CScript()// << ParseHex("76a9144f9ab47a237441c0dc61dcd39fd92709f281b03388ac");
                 // << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce,
                              nBits, nVersion, genesisReward);
}

CBlock CreateGenesisBlockTestnet(uint32_t nTime, uint32_t nNonce, uint32_t nBits,
                          int32_t nVersion, const Amount genesisReward) {
    const char *pszTimestamp =
        "This is a Radiant testing block";
    const CScript genesisOutputScript = CScript() << OP_DUP << OP_HASH160 << ParseHex("4f9ab47a237441c0dc61dcd39fd92709f281b033") << OP_EQUALVERIFY << OP_CHECKSIG;
        //CScript()// << ParseHex("76a9144f9ab47a237441c0dc61dcd39fd92709f281b03388ac");
                 // << OP_CHECKSIG;
    return CreateGenesisBlock(pszTimestamp, genesisOutputScript, nTime, nNonce,
                              nBits, nVersion, genesisReward);
}


/**
 * Main network
 */
class CMainParams : public CChainParams {
public:
    CMainParams() {
        strNetworkID = CBaseChainParams::MAIN;
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 17;
        consensus.BIP65Height = 100;
        consensus.BIP66Height = 101;
        consensus.CSVHeight = 102;
        consensus.ERHeight = 62000;
        consensus.PushTXStateHeight = 214555;
        consensus.powLimit = uint256S(
            "00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // one week
        consensus.nPowTargetTimespan = 7 * 24 * 60 * 60;
        consensus.nPowTargetSpacing = 5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = false;
        consensus.fPowNoRetargeting = false;

        // The half life for the ASERT DAA. For every (nASERTHalfLife) seconds behind schedule the blockchain gets,
        // difficulty is cut in half. Doubled if blocks are ahead of schedule.
        // Two days
        consensus.nASERTHalfLife = 2 * 24 * 60 * 60;

        // Sat Jul 09 2022 22:00:00 GMT+0000 ASERT DAA enabled
        consensus.asertActivationTime = 1657404000;

        // Anchor params: Note that the block after this height *must* also be checkpointed below.
        // Note: The anchor is searched dynamically automatically after consensus.asertActivationTime
        // The anchor params below will be added once that new block is mined and buried in the past
        // This is done so that the very first block after consensus.asertActivationTime will be anchored
        // to the original classic DAA right before it.
        consensus.asertAnchorParams = Consensus::Params::ASERTAnchor{
            18206,          // anchor block height
            0x1b03a760,     // anchor block nBits
            1657404650,     // anchor block previous block timestamp
        }; 

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork =
            ChainParamsConstants::MAINNET_MINIMUM_CHAIN_WORK;

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid =
            ChainParamsConstants::MAINNET_DEFAULT_ASSUME_VALID;

        // August 1, 2017 hard fork
        consensus.uahfHeight = 200;

        // Default limit for block size (in bytes)
        consensus.nDefaultExcessiveBlockSize = DEFAULT_EXCESSIVE_BLOCK_SIZE;

        // Chain-specific default for mining block size (in bytes) (configurable with -blockmaxsize)
        consensus.nDefaultGeneratedBlockSize = 16 * ONE_MEGABYTE;

        assert(consensus.nDefaultGeneratedBlockSize <= consensus.nDefaultExcessiveBlockSize);

        /**
         * The message start string is designed to be unlikely to occur in
         * normal data. The characters are rarely used upper ASCII, not valid as
         * UTF-8, and produce a large 32-bit integer with any alignment.
         */
        diskMagic[0] = 0xf9;
        diskMagic[1] = 0xbe;
        diskMagic[2] = 0xb4;
        diskMagic[3] = 0xd9;
        netMagic[0] = 0xe3;
        netMagic[1] = 0xe1;
        netMagic[2] = 0xf3;
        netMagic[3] = 0xe8;
        nDefaultPort = 7333;
        nPruneAfterHeight = 100000;
        m_assumed_blockchain_size = 240;
        m_assumed_chain_state_size = 5;
 
        genesis = CreateGenesisBlock(1655692970, 716669049, 0x1d00ffff, 1,
                                     50000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("0000000065d8ed5d8be28d6876b3ffb660ac2a6c0ca59e437e1f7a6f4e003fb4"));
        assert(genesis.hashMerkleRoot ==
               uint256S("027c3f6ae3858a75e2c287a915b3d82a50c48ee7021b715eedae9497f8ba2c37"));

        vSeeds.emplace_back("node.radiantblockchain.org");
        vSeeds.emplace_back("node.radiantone.org");
        vSeeds.emplace_back("node.radiantlayerone.com");
        vSeeds.emplace_back("radiant.bit90.io");
        vSeeds.emplace_back("radiant.relayx.com");
        vSeeds.emplace_back("us-chi1-rad.terraflow.info");
        vSeeds.emplace_back("uk-lon1-rad.terraflow.info");
        vSeeds.emplace_back("node.terraflow.info");
        vSeeds.emplace_back("radchad.ddns.net");
        vSeeds.emplace_back("node.radiantmideast.org");
        vSeeds.emplace_back("node6.radiant.ovh");
        vSeeds.emplace_back("node.radiant.ovh");
        vSeeds.emplace_back("node.radiantlayerone.com");
        vSeeds.emplace_back("node.radiantnode.org");
        vSeeds.emplace_back("node.radiantminer.com");
        vSeeds.emplace_back("node.radiantchain.org");
        vSeeds.emplace_back("node.radiantone.org");
        
        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 0);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 5);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 128);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x88, 0xB2, 0x1E};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x88, 0xAD, 0xE4};
        cashaddrPrefix = "radaddr";

        vFixedSeeds.assign(std::begin(pnSeed6_main), std::end(pnSeed6_main));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = false;

        checkpointData = {
            /* .mapCheckpoints = */ {
            {0, genesis.GetHash()},
            {10543, BlockHash::fromHex("0000000000389e57f64aeda459b441613dedb49b050ef0df1e25e4f325957dcf")}, 
            {18206, BlockHash::fromHex("000000000002b08f7be7bce01d53c4ab2f7b3420955866b88b3e6e087f941e07")}, // When ASERT went live
            {19068, BlockHash::fromHex("0000000000021a9928656e3b386b2e352e054fb2344ec0474e003cfabcdf0eff")}  // Latest
        }};

        // Data as of block
        // 000000000000000000d7e938f43eb520468fc75dc626c54ec770f9cd1bd6bc1d
        // (height 699219).
        chainTxData = ChainTxData{
            // UNIX timestamp of last known number of transactions.
            1628025092,
            // Total number of transactions between genesis and that timestamp
            // (the tx=... number in the ChainStateFlushed debug.log lines)
            337117246,
            // Estimated number of transactions per second after that timestamp.
            1.49,
        };
    }
};

/**
 * Testnet (v3)
 */
class CTestNetParams : public CChainParams {
public:
    CTestNetParams() {
        strNetworkID = CBaseChainParams::TESTNET;
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 0;
        consensus.BIP34Height = 17;
        consensus.BIP65Height = 100;
        consensus.BIP66Height = 101;
        consensus.CSVHeight = 102;
        consensus.ERHeight = 10600; 
        consensus.PushTXStateHeight = 182600;
        consensus.powLimit = uint256S(
            "00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // one week
        consensus.nPowTargetTimespan = 7 * 24 * 60 * 60;
        consensus.nPowTargetSpacing = 5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;

        // The half life for the ASERT DAA. For every (nASERTHalfLife) seconds behind schedule the blockchain gets,
        // difficulty is cut in half. Doubled if blocks are ahead of schedule.
        // 1 hour
        consensus.nASERTHalfLife = 60 * 60;

        // Sat Jul 09 2022 22:00:00 GMT+0000 ASERT DAA enabled
        consensus.asertActivationTime = 1657404000;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork =
            ChainParamsConstants::TESTNET_MINIMUM_CHAIN_WORK;

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid =
            ChainParamsConstants::MAINNET_DEFAULT_ASSUME_VALID;

        // August 1, 2017 hard fork
        consensus.uahfHeight = 200;

        // Default limit for block size (in bytes)
        consensus.nDefaultExcessiveBlockSize = DEFAULT_EXCESSIVE_BLOCK_SIZE;

        // Chain-specific default for mining block size (in bytes) (configurable with -blockmaxsize)
        consensus.nDefaultGeneratedBlockSize = 8 * ONE_MEGABYTE;

        assert(consensus.nDefaultGeneratedBlockSize <= consensus.nDefaultExcessiveBlockSize);

        diskMagic[0] = 0x0b;
        diskMagic[1] = 0x11;
        diskMagic[2] = 0x09;
        diskMagic[3] = 0x07;
        netMagic[0] = 0xf4;
        netMagic[1] = 0xe5;
        netMagic[2] = 0xf3;
        netMagic[3] = 0xf4;
        nDefaultPort = 17333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 60;
        m_assumed_chain_state_size = 2;

        genesis = CreateGenesisBlockTestnet(1657071137, 67046444, 0x1d00ffff, 1,
                                     50000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("000000002008a2f4a76b850a838ae084994c200dc2fd354f73102298fe063a91"));
        assert(genesis.hashMerkleRoot ==
               uint256S("364459380841db3f0ea491e8099bf98a6b7ffc5693d8ee6e46b3f8183e0257dc"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("node-testnet.radiantblockchain.org");
        vSeeds.emplace_back("node-testnet.radiantone.org");
        vSeeds.emplace_back("node-testnet.radiantlayerone.com");
        vSeeds.emplace_back("node-testnet.radiantchain.org");
        vSeeds.emplace_back("node-testnet.radiantnode.org");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        cashaddrPrefix = "radtest";
        vFixedSeeds.assign(std::begin(pnSeed6_testnet3), std::end(pnSeed6_testnet3));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;

        checkpointData = {
            /* .mapCheckpoints = */ {
                {0, genesis.GetHash()},

            }};

        // Data as of block
        // 0000000000002ad25634e653f5834f0c710fab41891dd696bf504262745e5cd5
        // (height 1459224)
        chainTxData = ChainTxData{1628025202, 63826727, 0.004631731783637};
    }
};

/**
 * Testnet (v4)
 */
class CTestNet4Params : public CChainParams {
public:
    CTestNet4Params() {
        strNetworkID = CBaseChainParams::TESTNET4;
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 1;
        // Note: Because BIP34Height is less than 17, clients will face an unusual corner case with BIP34 encoding.
        // The "correct" encoding for BIP34 blocks at height <= 16 uses OP_1 (0x81) through OP_16 (0x90) as a single
        // byte (i.e. "[shortest possible] encoded CScript format"), not a single byte with length followed by the
        // little-endian encoded version of the height as mentioned in BIP34. The BIP34 spec document itself ought to
        // be updated to reflect this.
        // https://github.com/bitcoin/bitcoin/pull/14633
        consensus.BIP34Height = 2;
        consensus.BIP65Height = 3;
        consensus.BIP66Height = 4;
        consensus.CSVHeight = 5;
        consensus.ERHeight = 10;
        consensus.PushTXStateHeight = 20;
        consensus.powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // one weeks
        consensus.nPowTargetTimespan = 7 * 24 * 60 * 60;
        consensus.nPowTargetSpacing = 5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;
 
        // The half life for the ASERT DAA. For every (nASERTHalfLife) seconds behind schedule the blockchain gets,
        // difficulty is cut in half. Doubled if blocks are ahead of schedule.
        // One hour
        consensus.nASERTHalfLife = 60 * 60;

        // Sat Jul 09 2022 22:00:00 GMT+0000 ASERT DAA enabled
        consensus.asertActivationTime = 1657404000;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = ChainParamsConstants::TESTNET4_MINIMUM_CHAIN_WORK;

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid = ChainParamsConstants::TESTNET4_DEFAULT_ASSUME_VALID;

        // August 1, 2017 hard fork
        consensus.uahfHeight = 6;

        // Default limit for block size (in bytes) (testnet4 is smaller at 2MB)
        consensus.nDefaultExcessiveBlockSize = 2 * ONE_MEGABYTE;

        // Chain-specific default for mining block size (in bytes) (configurable with -blockmaxsize)
        consensus.nDefaultGeneratedBlockSize = 2 * ONE_MEGABYTE;

        assert(consensus.nDefaultGeneratedBlockSize <= consensus.nDefaultExcessiveBlockSize);

        diskMagic[0] = 0xcd;
        diskMagic[1] = 0x22;
        diskMagic[2] = 0xa7;
        diskMagic[3] = 0x92;
        netMagic[0] = 0xe2;
        netMagic[1] = 0xb7;
        netMagic[2] = 0xda;
        netMagic[3] = 0xaf;
        nDefaultPort = 27333;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 1;
        m_assumed_chain_state_size = 1;

        genesis = CreateGenesisBlockTestnet(1657071785, 97033540, 0x1d00ffff, 1,
                                     50000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("000000000d8ada264d16f87a590b2af320cd3c7e3f9be5482163e830fd00aca2"));
        assert(genesis.hashMerkleRoot ==
               uint256S("364459380841db3f0ea491e8099bf98a6b7ffc5693d8ee6e46b3f8183e0257dc"));

        vFixedSeeds.clear();
        vSeeds.clear();
        // nodes with support for servicebits filtering should be at the top
        vSeeds.emplace_back("node-testnet4.radiantblockchain.org");
        vSeeds.emplace_back("node-testnet4.radiantone.org");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        cashaddrPrefix = "radtest";
        vFixedSeeds.assign(std::begin(pnSeed6_testnet4), std::end(pnSeed6_testnet4));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;

        checkpointData = {
            /* .mapCheckpoints = */ {
                {0, genesis.GetHash()},
            }};

        // Data as of block
        // 00000000009758d51aaf3bdc018b8b5c6e1725f742c850d44a0585ec168c409d
        // (height 54516)
        chainTxData = {1628025276, 56602, 0.001668541409299};
    }
};

/**
 * Scalenet
 */
class CScaleNetParams : public CChainParams {
public:
    CScaleNetParams() {
        strNetworkID = CBaseChainParams::SCALENET;
        consensus.nSubsidyHalvingInterval = 210000;
        consensus.BIP16Height = 1;
        consensus.BIP34Height = 2;
        consensus.BIP65Height = 3;
        consensus.BIP66Height = 4;
        consensus.CSVHeight = 5;
        consensus.ERHeight = 10;
        consensus.PushTXStateHeight = 20;
        consensus.powLimit = uint256S("00000000ffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // one weeks
        consensus.nPowTargetTimespan = 7 * 24 * 60 * 60;
        consensus.nPowTargetSpacing = 5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = false;

        // The half life for the ASERT DAA. For every (nASERTHalfLife) seconds behind schedule the blockchain gets,
        // difficulty is cut in half. Doubled if blocks are ahead of schedule.
        // Two days
        consensus.nASERTHalfLife = 2 * 24 * 60 * 60;

        // Sat Jul 09 2022 22:00:00 GMT+0000 ASERT DAA enabled
        consensus.asertActivationTime = 1657404000;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = ChainParamsConstants::SCALENET_MINIMUM_CHAIN_WORK;

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid = ChainParamsConstants::SCALENET_DEFAULT_ASSUME_VALID;

        // August 1, 2017 hard fork
        consensus.uahfHeight = 6;

        // Default limit for block size (in bytes)
        consensus.nDefaultExcessiveBlockSize = 256 * ONE_MEGABYTE;

        // Chain-specific default for mining block size (in bytes) (configurable with -blockmaxsize)
        consensus.nDefaultGeneratedBlockSize = 8 * ONE_MEGABYTE;

        assert(consensus.nDefaultGeneratedBlockSize <= consensus.nDefaultExcessiveBlockSize);

        diskMagic[0] = 0xba;
        diskMagic[1] = 0xc2;
        diskMagic[2] = 0x2d;
        diskMagic[3] = 0xc4;
        netMagic[0] = 0xc3;
        netMagic[1] = 0xaf;
        netMagic[2] = 0xe1;
        netMagic[3] = 0xa2;
        nDefaultPort = 37333;
        nPruneAfterHeight = 10000;
        m_assumed_blockchain_size = 200;
        m_assumed_chain_state_size = 20;

        genesis = CreateGenesisBlockTestnet(1657070732, 49505723, 0x1d00ffff, 1,
                                     50000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("00000000ea7340a6e9ae28ad8ca95a652c8da00ee7ea97e6cb42cd1558884c87"));
        assert(genesis.hashMerkleRoot ==
               uint256S("364459380841db3f0ea491e8099bf98a6b7ffc5693d8ee6e46b3f8183e0257dc"));

        vFixedSeeds.clear();
        vSeeds.clear();
        vSeeds.emplace_back("node-scalenet.radiantblockchain.org");
        vSeeds.emplace_back("node-scalenet.radiantone.org");

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        cashaddrPrefix = "radtest";
        vFixedSeeds.assign(std::begin(pnSeed6_scalenet), std::end(pnSeed6_scalenet));

        fDefaultConsistencyChecks = false;
        fRequireStandard = false;
        m_is_test_chain = true;

        checkpointData = {
            /* .mapCheckpoints = */ {
                {0, genesis.GetHash()},
            }};

        chainTxData = {0, 0, 0};
    }
};

/**
 * Regression test
 */
class CRegTestParams : public CChainParams {
public:
    CRegTestParams() {
        strNetworkID = CBaseChainParams::REGTEST;
        consensus.nSubsidyHalvingInterval = 150;
        // always enforce P2SH BIP16 on regtest RADIANT TODO: remove
        consensus.BIP16Height = 0;
        // BIP34 has not activated on regtest (far in the future so block v1 are
        // not rejected in tests)
        consensus.BIP34Height = 100000000;
        // BIP65 activated on regtest (Used in rpc activation tests)
        consensus.BIP65Height = 1351;
        // BIP66 activated on regtest (Used in rpc activation tests)
        consensus.BIP66Height = 1251;
        // CSV activated on regtest (Used in rpc activation tests)
        consensus.CSVHeight = 576;
        consensus.ERHeight = 100;
        consensus.PushTXStateHeight = 110;
        consensus.powLimit = uint256S(
            "7fffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        // one weeks
        consensus.nPowTargetTimespan = 7 * 24 * 60 * 60;
        consensus.nPowTargetSpacing = 5 * 60;
        consensus.fPowAllowMinDifficultyBlocks = true;
        consensus.fPowNoRetargeting = true;

        // The best chain should have at least this much work.
        consensus.nMinimumChainWork = uint256S("0x00");

        // By default assume that the signatures in ancestors of this block are
        // valid.
        consensus.defaultAssumeValid = BlockHash();

        // UAHF is always enabled on regtest.
        consensus.uahfHeight = 0;

        // Default limit for block size (in bytes)
        consensus.nDefaultExcessiveBlockSize = DEFAULT_EXCESSIVE_BLOCK_SIZE;

        // Chain-specific default for mining block size (in bytes) (configurable with -blockmaxsize)
        consensus.nDefaultGeneratedBlockSize = 8 * ONE_MEGABYTE;

        assert(consensus.nDefaultGeneratedBlockSize <= consensus.nDefaultExcessiveBlockSize);

        diskMagic[0] = 0xfa;
        diskMagic[1] = 0xbf;
        diskMagic[2] = 0xb5;
        diskMagic[3] = 0xda;
        netMagic[0] = 0xda;
        netMagic[1] = 0xb5;
        netMagic[2] = 0xbf;
        netMagic[3] = 0xfa;
        nDefaultPort = 18444;
        nPruneAfterHeight = 1000;
        m_assumed_blockchain_size = 0;
        m_assumed_chain_state_size = 0;

        genesis = CreateGenesisBlockTestnet(1657071137, 67046444, 0x1d00ffff, 1,
                                     50000 * COIN);
        consensus.hashGenesisBlock = genesis.GetHash();
        assert(consensus.hashGenesisBlock ==
               uint256S("000000002008a2f4a76b850a838ae084994c200dc2fd354f73102298fe063a91"));
        assert(genesis.hashMerkleRoot ==
               uint256S("364459380841db3f0ea491e8099bf98a6b7ffc5693d8ee6e46b3f8183e0257dc"));

        //! Regtest mode doesn't have any fixed seeds.
        vFixedSeeds.clear();
        //! Regtest mode doesn't have any DNS seeds.
        vSeeds.clear();

        fDefaultConsistencyChecks = true;
        fRequireStandard = false;
        m_is_test_chain = true;

        checkpointData = {
            /* .mapCheckpoints = */ {
                {0, genesis.GetHash()},
            }};

        chainTxData = ChainTxData{0, 0, 0};

        base58Prefixes[PUBKEY_ADDRESS] = std::vector<uint8_t>(1, 111);
        base58Prefixes[SCRIPT_ADDRESS] = std::vector<uint8_t>(1, 196);
        base58Prefixes[SECRET_KEY] = std::vector<uint8_t>(1, 239);
        base58Prefixes[EXT_PUBLIC_KEY] = {0x04, 0x35, 0x87, 0xCF};
        base58Prefixes[EXT_SECRET_KEY] = {0x04, 0x35, 0x83, 0x94};
        cashaddrPrefix = "radreg";
    }
};


static std::unique_ptr<CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<CChainParams> CreateChainParams(const std::string &chain) {
    if (chain == CBaseChainParams::MAIN) {
        return std::make_unique<CMainParams>();
    }

    if (chain == CBaseChainParams::TESTNET) {
        return std::make_unique<CTestNetParams>();
    }

    if (chain == CBaseChainParams::TESTNET4) {
        return std::make_unique<CTestNet4Params>();
    }

    if (chain == CBaseChainParams::REGTEST) {
        return std::make_unique<CRegTestParams>();
    }

    if (chain == CBaseChainParams::SCALENET) {
        return std::make_unique<CScaleNetParams>();
    }

    throw std::runtime_error(
        strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string &network) {
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(network);
}

SeedSpec6::SeedSpec6(const char *pszHostPort)
{
    const CService service = LookupNumeric(pszHostPort, 0);
    if (!service.IsValid() || service.GetPort() == 0)
        throw std::invalid_argument(strprintf("Unable to parse numeric-IP:port pair: %s", pszHostPort));
    if (!service.IsRoutable())
        throw std::invalid_argument(strprintf("Not routable: %s", pszHostPort));
    *this = SeedSpec6(service);
}
