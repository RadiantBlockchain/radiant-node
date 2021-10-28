// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/forms/ui_legacyaddressstopdialog.h>
#include <qt/forms/ui_legacyaddresswarndialog.h>
#include <qt/legacyaddressdialog.h>

LegacyAddressStopDialog::LegacyAddressStopDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LegacyAddressStopDialog) {
    ui->setupUi(this);
}

LegacyAddressStopDialog::~LegacyAddressStopDialog() {
    delete ui;
}

void LegacyAddressStopDialog::SetAddressType(LegacyAddressType type) {
    switch(type) {
    case P2PKH:
        ui->p2pkh_label->show();
        break;
    case P2SH:
        ui->p2sh_label->show();
        break;
    }
    adjustSize();
}

LegacyAddressWarnDialog::LegacyAddressWarnDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LegacyAddressWarnDialog) {
    ui->setupUi(this);
}

LegacyAddressWarnDialog::~LegacyAddressWarnDialog() {
    delete ui;
}

void LegacyAddressWarnDialog::SetAddressType(LegacyAddressType type) {
    switch(type) {
    case P2PKH:
        ui->p2pkh_label->show();
        break;
    case P2SH:
        ui->p2sh_label->show();
        break;
    }
    adjustSize();
}
