// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_LEGACYADDRESSCONVERTDIALOG_H
#define BITCOIN_QT_LEGACYADDRESSCONVERTDIALOG_H

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

#endif // BITCOIN_QT_LEGACYADDRESSCONVERTDIALOG_H
