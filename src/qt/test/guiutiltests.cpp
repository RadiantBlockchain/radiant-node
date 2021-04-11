// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/test/guiutiltests.h>

#include <chainparams.h>
#include <config.h>
#include <key_io.h>
#include <qt/guiutil.h>
#include <qt/optionsmodel.h>

namespace {

class GUIUtilTestConfig : public DummyConfig {
public:
    GUIUtilTestConfig()
        : DummyConfig(CBaseChainParams::MAIN), useCashAddr(true) {}
    void SetCashAddrEncoding(bool b) override { useCashAddr = b; }
    bool UseCashAddrEncoding() const override { return useCashAddr; }

private:
    bool useCashAddr;
};

} // namespace

void GUIUtilTests::dummyAddressTest() {

    GUIUtilTestConfig config;
    const CChainParams &params = config.GetChainParams();

    std::string dummyaddr;

    dummyaddr = GUIUtil::DummyAddress(params);
    QVERIFY(!IsValidDestinationString(dummyaddr, params));
    QVERIFY(!dummyaddr.empty());
}

void GUIUtilTests::toCurrentEncodingTest() {
    GUIUtilTestConfig config;
    const CChainParams &params = config.GetChainParams();

    // garbage in, garbage out
    QVERIFY(GUIUtil::convertToCashAddr(params, "garbage") == "garbage");

    QString cashaddr_pubkey =
        "bitcoincash:qpm2qsznhks23z7629mms6s4cwef74vcwvy22gdx6a";
    QString base58_pubkey = "1BpEi6DfDAUFd7GtittLSdBeYJvcoaVggu";

    QVERIFY(GUIUtil::convertToCashAddr(params, cashaddr_pubkey) ==
            cashaddr_pubkey);
    QVERIFY(GUIUtil::convertToCashAddr(params, base58_pubkey) ==
            cashaddr_pubkey);
}

void GUIUtilTests::formatBytesTest() {
    QLocale::setDefault(QLocale("en_US"));
    QVERIFY(GUIUtil::formatBytes(            0) == "0 B");
    QVERIFY(GUIUtil::formatBytes(            1) == "1 B");
    QVERIFY(GUIUtil::formatBytes(          999) == "999 B");
    QVERIFY(GUIUtil::formatBytes(        1'000) == "1 kB");
    QVERIFY(GUIUtil::formatBytes(        1'999) == "1 kB");
    QVERIFY(GUIUtil::formatBytes(        2'000) == "2 kB");
    QVERIFY(GUIUtil::formatBytes(      999'000) == "999 kB");
    QVERIFY(GUIUtil::formatBytes(    1'000'000) == "1.0 MB");
    QVERIFY(GUIUtil::formatBytes(    1'099'999) == "1.0 MB");
    QVERIFY(GUIUtil::formatBytes(    1'100'000) == "1.1 MB");
    QVERIFY(GUIUtil::formatBytes(    1'999'999) == "1.9 MB");
    QVERIFY(GUIUtil::formatBytes(    2'000'000) == "2.0 MB");
    QVERIFY(GUIUtil::formatBytes(  999'999'999) == "999.9 MB");
    QVERIFY(GUIUtil::formatBytes(1'000'000'000) == "1.00 GB");
    QVERIFY(GUIUtil::formatBytes(1'009'999'999) == "1.00 GB");
    QVERIFY(GUIUtil::formatBytes(1'010'000'000) == "1.01 GB");
    QVERIFY(GUIUtil::formatBytes(1'099'999'999) == "1.09 GB");
    QVERIFY(GUIUtil::formatBytes(1'100'000'000) == "1.10 GB");
    QVERIFY(GUIUtil::formatBytes(1'999'999'999) == "1.99 GB");
    QVERIFY(GUIUtil::formatBytes(2'000'000'000) == "2.00 GB");
    QLocale::setDefault(QLocale("de_DE"));
    QVERIFY(GUIUtil::formatBytes(    1'000'000) == "1,0 MB");
    QVERIFY(GUIUtil::formatBytes(    1'099'999) == "1,0 MB");
    QVERIFY(GUIUtil::formatBytes(    1'100'000) == "1,1 MB");
    QVERIFY(GUIUtil::formatBytes(    1'999'999) == "1,9 MB");
    QVERIFY(GUIUtil::formatBytes(    2'000'000) == "2,0 MB");
    QVERIFY(GUIUtil::formatBytes(  999'999'999) == "999,9 MB");
    QVERIFY(GUIUtil::formatBytes(1'000'000'000) == "1,00 GB");
    QVERIFY(GUIUtil::formatBytes(1'009'999'999) == "1,00 GB");
    QVERIFY(GUIUtil::formatBytes(1'010'000'000) == "1,01 GB");
    QVERIFY(GUIUtil::formatBytes(1'099'999'999) == "1,09 GB");
    QVERIFY(GUIUtil::formatBytes(1'100'000'000) == "1,10 GB");
    QVERIFY(GUIUtil::formatBytes(1'999'999'999) == "1,99 GB");
    QVERIFY(GUIUtil::formatBytes(2'000'000'000) == "2,00 GB");
}


void GUIUtilTests::txViewerURLValidationTest() {
    QLocale::setDefault(QLocale("en_US"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString(""));  // special case of empty URL is allowed
    QVERIFY(!OptionsModel::isValidThirdPartyTxUrlString("foo"));
    QVERIFY(!OptionsModel::isValidThirdPartyTxUrlString("123.123.123.123"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("http://foo.com"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("http:/foo.com"));  // ?
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("http://foo.com/%s"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("https://foo.com"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("https://foo.com/%s"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("https:/foo.com"));  // ?
    QVERIFY(!OptionsModel::isValidThirdPartyTxUrlString("ftp://foo.com/path"));
    QVERIFY(!OptionsModel::isValidThirdPartyTxUrlString("ssh://foo.com/path"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("HTTP://foo.com/path"));
    QVERIFY(OptionsModel::isValidThirdPartyTxUrlString("HTTPS://foo.com/path"));
}
