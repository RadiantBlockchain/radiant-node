// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QDialog>

enum LegacyAddressType {
    P2PKH,
    P2SH
};

namespace Ui {
class LegacyAddressStopDialog;
class LegacyAddressWarnDialog;
}

class LegacyAddressStopDialog : public QDialog {
    Q_OBJECT

public:
    explicit LegacyAddressStopDialog(QWidget *parent);
    ~LegacyAddressStopDialog();
    void SetAddressType(LegacyAddressType type);

private:
   Ui::LegacyAddressStopDialog *ui;
};

class LegacyAddressWarnDialog : public QDialog {
    Q_OBJECT

public:
    explicit LegacyAddressWarnDialog(QWidget *parent);
    ~LegacyAddressWarnDialog();
    void SetAddressType(LegacyAddressType type);

private:
   Ui::LegacyAddressWarnDialog *ui;
};
