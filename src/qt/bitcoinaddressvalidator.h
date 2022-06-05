// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <key_io.h>

#include <QValidator>

/**
 * Radiant address entry widget validator, checks for valid characters and
 * removes some whitespace.
 */
class BitcoinAddressEntryValidator : public QValidator {
    Q_OBJECT

public:
    explicit BitcoinAddressEntryValidator(const std::string &cashaddrprefixIn,
                                          QObject *parent);

    State validate(QString &input, int &pos) const override;

private:
    std::string cashaddrprefix;
};

/** Radiant address widget validator, checks for a valid Radiant address.
 */
class BitcoinAddressCheckValidator : public QValidator {
    Q_OBJECT

public:
    explicit BitcoinAddressCheckValidator(QWidget *parent);

    /** @name Methods overridden from QValidator
    @{*/
    State validate(QString &input, int &pos) const override;
    void fixup(QString &input) const override;
    /*@}*/

private:
    bool GetLegacyAddressUseAuth(const CTxDestination &destination) const;
    bool GetLegacyAddressConversionAuth(const QString &original,
                                        const QString &normalized) const;
    QWidget* parentWidget() const;
};
