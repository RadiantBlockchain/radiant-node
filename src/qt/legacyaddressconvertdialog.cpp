// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/forms/ui_legacyaddressconvertdialog.h>
#include <qt/legacyaddressconvertdialog.h>

LegacyAddressConvertDialog::LegacyAddressConvertDialog(QWidget *parent)
    : QDialog(parent), ui(new Ui::LegacyAddressConvertDialog) {
    ui->setupUi(this);
}

LegacyAddressConvertDialog::~LegacyAddressConvertDialog() {
    delete ui;
}

void LegacyAddressConvertDialog::SetAddresses(const QString &legacyAddr, const QString &newAddr) {
    ui->label_2->setText(ui->label_2->text().arg(legacyAddr, newAddr));
}
