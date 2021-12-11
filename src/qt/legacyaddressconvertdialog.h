// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QDialog>

class CChainParams;

namespace Ui {
class LegacyAddressConvertDialog;
}

class LegacyAddressConvertDialog : public QDialog {
    Q_OBJECT

public:
    explicit LegacyAddressConvertDialog(QWidget *parent);
    ~LegacyAddressConvertDialog();
    void SetAddresses(const QString &legacyAddr, const QString &newAddr);

private:
   Ui::LegacyAddressConvertDialog *ui;
};
