// Copyright (c) 2018 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <logging.h>
#include <util/system.h>
#include <walletinitinterface.h>

class CWallet;

class DummyWalletInit : public WalletInitInterface {
public:
    bool HasWalletSupport() const override { return false; }
    void AddWalletOptions() const override;
    bool ParameterInteraction() const override { return true; }
    void Construct(NodeContext &node) const override {
        LogPrintf("No wallet support compiled in!\n");
    }
};

void DummyWalletInit::AddWalletOptions() const {
    std::vector<std::string> opts = {
        "-avoidpartialspends", "-disablewallet",
        "-fallbackfee=<amt>", "-keypool=<n>", "-maxtxfee=<amt>",
        "-mintxfee=<amt>", "-paytxfee=<amt>", "-rescan", "-salvagewallet",
        "-spendzeroconfchange", "-upgradewallet", "-wallet=<path>",
        "-walletbroadcast", "-walletdir=<dir>", "-walletnotify=<cmd>",
        "-zapwallettxes=<mode>",
        // Wallet debug options
        "-dblogsize=<n>", "-flushwallet", "-privdb"};
    gArgs.AddHiddenArgs(opts);
}

const WalletInitInterface &g_wallet_init_interface = DummyWalletInit();

fs::path GetWalletDir() {
    throw std::logic_error("Wallet function called in non-wallet build.");
}

std::vector<fs::path> ListWalletDir() {
    throw std::logic_error("Wallet function called in non-wallet build.");
}

std::vector<std::shared_ptr<CWallet>> GetWallets() {
    throw std::logic_error("Wallet function called in non-wallet build.");
}

namespace interfaces {

class Wallet;

std::unique_ptr<Wallet> MakeWallet(const std::shared_ptr<CWallet> &wallet) {
    throw std::logic_error("Wallet function called in non-wallet build.");
}

} // namespace interfaces
