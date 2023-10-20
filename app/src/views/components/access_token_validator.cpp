#include "access_token_validator.h"

#include <QtWidgets/qwidget.h>

AccesstokenValidator::AccesstokenValidator(QWidget* parent)
    : QValidator(parent) {}

QValidator::State AccesstokenValidator::validate(QString& input, int& pos) const {
    input = input.trimmed();
    if (input.length() > 6) {
        input.remove(6, input.length() - 6);
        pos = std::min(pos, 6);
    }
    size_t size = static_cast<size_t>(input.size());
    for (size_t i = 0; i < size; i++) {
        if ((input[i] >= 'a' && input[i] <= 'z') || (input[i] >= '0' && input[i] <= '9')) {
            continue;
        }
        if (input[i] >= 'A' && input[i] <= 'Z') {
            input[i] = input[i].toLower();
            continue;
        }
        return State::Invalid;
    }
    return State::Acceptable;
}

void AccesstokenValidator::fixup(QString& input) const {
    input = input.toUpper();
}
