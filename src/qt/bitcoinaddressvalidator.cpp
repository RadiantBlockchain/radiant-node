// Copyright (c) 2011-2014 The Bitcoin Core developers
// Copyright (c) 2017-2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/bitcoinaddressvalidator.h>

#include <config.h>
#include <key_io.h>

/* Base58 characters are:
     "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz"

  This is:
  - All numbers except for '0'
  - All upper-case letters except for 'I' and 'O'
  - All lower-case letters except for 'l'
*/
BitcoinAddressEntryValidator::BitcoinAddressEntryValidator(
    const std::string &cashaddrprefixIn, QObject *parent)
    : QValidator(parent), cashaddrprefix(cashaddrprefixIn) {}

QValidator::State BitcoinAddressEntryValidator::validate(QString &input, [[maybe_unused]] int &pos) const {

    // Empty address is "intermediate" input
    if (input.isEmpty()) {
        return QValidator::Intermediate;
    }

    // Correction
    for (int idx = 0; idx < input.size();) {
        bool removeChar = false;
        QChar ch = input.at(idx);
        // Corrections made are very conservative on purpose, to avoid
        // users unexpectedly getting away with typos that would normally
        // be detected, and thus sending to the wrong address.
        switch (ch.unicode()) {
            // Qt categorizes these as "Other_Format" not "Separator_Space"
            case 0x200B: // ZERO WIDTH SPACE
            case 0xFEFF: // ZERO WIDTH NO-BREAK SPACE
                removeChar = true;
                break;
            default:
                break;
        }

        // Remove whitespace
        if (ch.isSpace()) {
            removeChar = true;
        }

        // To next character
        if (removeChar) {
            input.remove(idx, 1);
        } else {
            ++idx;
        }
    }

    // Validation
    for (int idx = 0; idx < input.size(); ++idx) {
        int ch = input.at(idx).unicode();

        if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'z') && (ch < 'A' || ch > 'Z') && ch != ':') {
            // Only alphanumeric allowed.
            // We also include ':' for cashaddr.
            return QValidator::Invalid;
        }
    }

    return QValidator::Acceptable;
}

BitcoinAddressCheckValidator::BitcoinAddressCheckValidator(QObject *parent)
    : QValidator(parent) {}

QValidator::State BitcoinAddressCheckValidator::validate(QString &input, [[maybe_unused]] int &pos) const {

    // Validate the passed Bitcoin Cash address
    CTxDestination destination = DecodeDestination(input.toStdString(), GetConfig().GetChainParams());
    if (IsValidDestination(destination)) {
        // Address is valid
        // Normalize address notation (e.g. convert to CashAddr/Base58, add CashAddr prefix, uppercase CashAddr to lowercase)
        input = QString::fromStdString(EncodeDestination(destination, GetConfig()));
        return QValidator::Acceptable;
    }

    return QValidator::Invalid;
}
