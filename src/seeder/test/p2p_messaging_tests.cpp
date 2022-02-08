// Copyright (c) 2019-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#define BOOST_TEST_MODULE Bitcoin Seeder Test Suite

#include <chainparams.h>
#include <protocol.h>
#include <seeder/bitcoin.h>
#include <seeder/db.h>
#include <seeder/test/util.h>
#include <serialize.h>
#include <streams.h>
#include <util/system.h>
#include <version.h>
#include <test/setup_common.h>
#include <validation.h>

#include <ctime>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

std::ostream &operator<<(std::ostream &os, const PeerMessagingState &state) {
    os << to_integral(state);
    return os;
}

namespace {
class CSeederNodeTest : public CSeederNode {
public:
    CSeederNodeTest(const CService &service, std::vector<CAddress> *vAddrIn)
        : CSeederNode(service, vAddrIn) {}

    void TestProcessMessage(const std::string &msg_type, CDataStream &message,
                            PeerMessagingState expectedState) {
        PeerMessagingState ret = ProcessMessage(msg_type, message);
        BOOST_CHECK_EQUAL(ret, expectedState);
    }

    CDataStream getSendBuffer() { return vSend; }
};
} // namespace

static const unsigned short SERVICE_PORT = 18444;

struct SeederTestingSetup : public TestChain100Setup {
    SeederTestingSetup() {
        CNetAddr ip;
        ip.SetInternal("bitcoin.test");
        CService service = {ip, SERVICE_PORT};
        vAddr.emplace_back(service, ServiceFlags());
        testNode = std::make_unique<CSeederNodeTest>(service, &vAddr);
    }

    std::vector<CAddress> vAddr;
    std::unique_ptr<CSeederNodeTest> testNode;
};

BOOST_FIXTURE_TEST_SUITE(p2p_messaging_tests, SeederTestingSetup)

static constexpr int OUR_VERSION = PROTOCOL_VERSION;
static constexpr const char *OUR_SUBVERSION = "/custom-useragent/";

static CDataStream
CreateVersionMessage(int64_t now, CAddress addrTo, CAddress addrFrom,
                     int32_t start_height, uint32_t nVersion = OUR_VERSION,
                     uint64_t nonce = 0, std::string user_agent = OUR_SUBVERSION) {
    CDataStream payload(SER_NETWORK, 0);
    payload.SetVersion(INIT_PROTO_VERSION);
    ServiceFlags serviceflags = ServiceFlags(NODE_NETWORK);
    payload << nVersion << uint64_t(serviceflags) << now << addrTo << addrFrom
            << nonce << user_agent << start_height;
    return payload;
}

static const int SEEDER_INIT_VERSION = 0;

BOOST_AUTO_TEST_CASE(process_version_msg) {
    CService serviceFrom;
    CAddress addrFrom(serviceFrom,
                      ServiceFlags(NODE_NETWORK | NODE_BITCOIN_CASH));

    CDataStream versionMessage = CreateVersionMessage(std::time(nullptr), vAddr[0], addrFrom, GetRequireHeight());

    // Verify the version is set as the initial value
    BOOST_CHECK_EQUAL(testNode->CSeederNode::GetClientVersion(),
                      SEEDER_INIT_VERSION);
    BOOST_CHECK_EQUAL(testNode->GetClientSubVersion(), "");
    testNode->TestProcessMessage(NetMsgType::VERSION, versionMessage,
                                 PeerMessagingState::AwaitingMessages);
    // Verify the version has been updated
    BOOST_CHECK_EQUAL(testNode->CSeederNode::GetClientVersion(), OUR_VERSION);
    // Also verify the subversion has been updated
    BOOST_CHECK_EQUAL(testNode->GetClientSubVersion(), OUR_SUBVERSION);

    // Seeder should respond with a SENDADDRV2 message, then a VERACK
    const CMessageHeader::MessageMagic netMagic = Params().NetMagic();
    CMessageHeader header(netMagic);
    CDataStream sendBuffer = testNode->getSendBuffer();
    sendBuffer >> header;
    BOOST_CHECK(header.IsValidWithoutConfig(netMagic));
    BOOST_CHECK_EQUAL(header.GetCommand(), NetMsgType::SENDADDRV2);

    // next, VERACK
    sendBuffer >> header;
    BOOST_CHECK(header.IsValidWithoutConfig(netMagic));
    BOOST_CHECK_EQUAL(header.GetCommand(), NetMsgType::VERACK);
}

BOOST_AUTO_TEST_CASE(process_verack_msg) {
    CDataStream verackMessage(SER_NETWORK, 0);
    verackMessage.SetVersion(OUR_VERSION);
    testNode->TestProcessMessage(NetMsgType::VERACK, verackMessage,
                                 PeerMessagingState::AwaitingMessages);

    // Seeder should respond with a GETADDR message
    const CMessageHeader::MessageMagic netMagic = Params().NetMagic();
    CMessageHeader header(netMagic);
    CDataStream sendBuffer = testNode->getSendBuffer();
    sendBuffer >> header;
    BOOST_CHECK(header.IsValidWithoutConfig(netMagic));
    BOOST_CHECK_EQUAL(header.GetCommand(), NetMsgType::GETADDR);

    // Next message should be GETHEADERS
    sendBuffer >> header;
    BOOST_CHECK(header.IsValidWithoutConfig(netMagic));
    BOOST_CHECK_EQUAL(header.GetCommand(), NetMsgType::GETHEADERS);

    CBlockLocator locator;
    uint256 hashStop;
    sendBuffer >> locator >> hashStop;
    std::vector<BlockHash> expectedLocator = {
        Params().Checkpoints().mapCheckpoints.rbegin()->second};
    BOOST_CHECK(locator.vHave == expectedLocator);
    BOOST_CHECK(hashStop == uint256());
}

BOOST_AUTO_TEST_CASE(process_headers_msg) {
    CService serviceFrom;
    CAddress addrFrom(serviceFrom,
                      ServiceFlags(NODE_NETWORK | NODE_BITCOIN_CASH));

    CDataStream versionMessage = CreateVersionMessage(std::time(nullptr), vAddr[0], addrFrom, GetRequireHeight() + 1);

    testNode->TestProcessMessage(NetMsgType::VERSION, versionMessage,
                                 PeerMessagingState::AwaitingMessages);

    auto blockOneHeader = ::ChainActive()[1]->GetBlockHeader();

    CDataStream headersMessage(SER_NETWORK, 0);
    headersMessage.SetVersion(testNode->GetClientVersion());
    WriteCompactSize(headersMessage, 1);
    headersMessage << blockOneHeader;
    WriteCompactSize(headersMessage, 0);

    testNode->TestProcessMessage(NetMsgType::HEADERS, headersMessage,
                                 PeerMessagingState::AwaitingMessages);
    BOOST_CHECK(testNode->GetBan() == 0);

    auto badBlockOneHeader = CBlockHeader();

    CDataStream badHeadersMessage(SER_NETWORK, 0);
    badHeadersMessage.SetVersion(testNode->GetClientVersion());
    WriteCompactSize(badHeadersMessage, 1);
    badHeadersMessage << badBlockOneHeader;
    WriteCompactSize(badHeadersMessage, 0);

    testNode->TestProcessMessage(NetMsgType::HEADERS, badHeadersMessage,
                                 PeerMessagingState::Finished);
    BOOST_CHECK(testNode->GetBan() > 0);
}

BOOST_AUTO_TEST_CASE(ban_too_many_headers) {
    auto blockOneHeader = ::ChainActive()[1]->GetBlockHeader();

    CDataStream tooManyHeadersMessage(SER_NETWORK, 0);
    tooManyHeadersMessage.SetVersion(testNode->GetClientVersion());
    WriteCompactSize(tooManyHeadersMessage, 2001);
    for (size_t i = 0; i < 2001; i++) {
        tooManyHeadersMessage << blockOneHeader;
        WriteCompactSize(tooManyHeadersMessage, 0);
    }

    testNode->TestProcessMessage(NetMsgType::HEADERS, tooManyHeadersMessage,
                                 PeerMessagingState::Finished);
    BOOST_CHECK(testNode->GetBan() > 0);
}

static CDataStream CreateAddrMessage(const std::vector<CAddress> &sendAddrs, bool isAddrV2) {
    CDataStream payload(SER_NETWORK, 0);
    payload.SetVersion(isAddrV2 ? OUR_VERSION | ADDRV2_FORMAT : OUR_VERSION);
    payload << sendAddrs;
    return payload;
}

// Test that seeder responds to both ADDR and ADDRV2 messages
BOOST_AUTO_TEST_CASE(process_addr_msg) {
    for (auto [msg_type, isV2] : {std::pair(NetMsgType::ADDR, false), std::pair(NetMsgType::ADDRV2, true)}) {
        // vAddrs starts with 1 entry.
        std::vector<CAddress> sendAddrs(ADDR_SOFT_CAP - 1, vAddr[0]);

        // Happy path
        // addrs are added normally to vAddr until ADDR_SOFT_CAP is reached.
        // Add addrs up to the soft cap.
        CDataStream addrMessage = CreateAddrMessage(sendAddrs, isV2);
        BOOST_CHECK_EQUAL(1, vAddr.size());
        testNode->TestProcessMessage(msg_type, addrMessage,
                                     PeerMessagingState::AwaitingMessages);
        BOOST_CHECK_EQUAL(ADDR_SOFT_CAP, vAddr.size());

        // ADDR_SOFT_CAP is exceeded
        sendAddrs.resize(1);
        addrMessage = CreateAddrMessage(sendAddrs, isV2);
        testNode->TestProcessMessage(msg_type, addrMessage,
                                     PeerMessagingState::Finished);
        BOOST_CHECK_EQUAL(ADDR_SOFT_CAP + 1, vAddr.size());

        // Test the seeder's behavior after ADDR_SOFT_CAP addrs
        // Only one addr per ADDR message will be added, the rest are ignored
        size_t expectedSize = vAddr.size() + 1;
        for (size_t i = 1; i < 10; i++) {
            sendAddrs.resize(i, sendAddrs[0]);
            addrMessage = CreateAddrMessage(sendAddrs, isV2);
            testNode->TestProcessMessage(msg_type, addrMessage,
                                         PeerMessagingState::Finished);
            BOOST_CHECK_EQUAL(expectedSize, vAddr.size());
            ++expectedSize;
        }

        // reset vAddr for next iteration
        vAddr.resize(1);
    }
}

BOOST_AUTO_TEST_SUITE_END()
