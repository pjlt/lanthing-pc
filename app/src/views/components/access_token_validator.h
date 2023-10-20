#pragma once
#include <QValidator>

class AccesstokenValidator : public QValidator {
public:
    AccesstokenValidator(QWidget* parent);
    State validate(QString&, int&) const override;
    void fixup(QString&) const override;
};