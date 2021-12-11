// Copyright (c) 2011-2015 The Bitcoin Core developers
// Copyright (c) 2021 The Bitcoin developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#pragma once

#include <QLineEdit>
#include <QValidator>

/** Line edit that can be marked as "invalid" to show input validation feedback.
   When marked as invalid,
   it will get a red background until it is focused.
 */
class QValidatedLineEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit QValidatedLineEdit(QWidget *parent);
    void clear();
    void setCheckValidator(const QValidator *v);
    bool isValid();

protected:
    void focusInEvent(QFocusEvent *evt) override;
    void focusOutEvent(QFocusEvent *evt) override;

private:
    QValidator::State state;
    const QValidator *checkValidator;

public Q_SLOTS:
    void setValid(bool valid=true);
    void setValid(QValidator::State _state);
    void setEnabled(bool enabled);
    bool validate();
    void fixup();

Q_SIGNALS:
    void validationDidChange(QValidatedLineEdit *validatedLineEdit);

private Q_SLOTS:
    void markValid();
};
