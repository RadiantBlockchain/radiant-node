// Copyright (c) 2011-2016 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/qvalidatedlineedit.h>

#include <config.h>
#include <qt/bitcoinaddressvalidator.h>
#include <qt/guiconstants.h>

QValidatedLineEdit::QValidatedLineEdit(QWidget *parent)
    : QLineEdit(parent), state(QValidator::Acceptable), checkValidator(nullptr) {
    connect(this, &QValidatedLineEdit::textChanged, this,
            &QValidatedLineEdit::markValid);
}

void QValidatedLineEdit::setValid(bool valid) {
    setValid(valid ? QValidator::Acceptable : QValidator::Invalid);
}

void QValidatedLineEdit::setValid(QValidator::State _state) {
    if (_state == this->state) {
        return;
    }

    if (_state == QValidator::Acceptable) {
        setStyleSheet("");
    } else if (_state == QValidator::Intermediate) {
        setStyleSheet(STYLE_INTERMEDIATE);
    } else {
        setStyleSheet(STYLE_INVALID);
    }
    this->state = _state;
}

void QValidatedLineEdit::focusInEvent(QFocusEvent *evt) {
    // Clear invalid flag on focus
    setValid(true);

    QLineEdit::focusInEvent(evt);
}

void QValidatedLineEdit::focusOutEvent(QFocusEvent *evt) {
    validate();

    QLineEdit::focusOutEvent(evt);
}

void QValidatedLineEdit::markValid() {
    // As long as a user is typing ensure we display state as valid
    setValid(true);
}

void QValidatedLineEdit::clear() {
    setValid(true);
    QLineEdit::clear();
}

void QValidatedLineEdit::setEnabled(bool enabled) {
    if (!enabled) {
        // A disabled QValidatedLineEdit should be marked valid
        setValid(true);
    } else {
        // Recheck validity when QValidatedLineEdit gets enabled
        validate();
    }

    QLineEdit::setEnabled(enabled);
}

bool QValidatedLineEdit::validate() {
    QString input = text();
    QString origInput = input;
    if (input.isEmpty()) {
        setValid(true);
    } else if (hasAcceptableInput()) {
        setValid(true);
        if (checkValidator) {
            int pos = 0;
            setValid(checkValidator->validate(input, pos));
            // checkValidator may have modified the text, if so update the text
            if (input != origInput) {
                setText(input);
            }
        }
    } else {
        setValid(false);
    }

    Q_EMIT validationDidChange(this);

    return state == QValidator::Acceptable;
}

void QValidatedLineEdit::fixup() {
    if (checkValidator) {
        QString input = text();
        QString origInput = input;
        checkValidator->fixup(input);
        if (input != origInput) {
            setText(input);
        }
    }
}

void QValidatedLineEdit::setCheckValidator(const QValidator *v) {
    checkValidator = v;
}

bool QValidatedLineEdit::isValid() {
    // use checkValidator in case the QValidatedLineEdit is disabled
    if (checkValidator) {
        QString input = text();
        int pos = 0;
        if (checkValidator->validate(input, pos) == QValidator::Acceptable) {
            return true;
        }
    }

    return state == QValidator::Acceptable;
}
