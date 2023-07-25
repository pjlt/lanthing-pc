#include "mainpage.h"

#include <QAction>
#include <QValidator>

#include "app.h"
#include "ui_mainpage.h"

namespace {

class AccesstokenValidator : public QValidator {
public:
    AccesstokenValidator(QWidget* parent);
    State validate(QString&, int&) const override;
    // void fixup(QString&) const override;
};

AccesstokenValidator::AccesstokenValidator(QWidget* parent)
    : QValidator(parent) {}

QValidator::State AccesstokenValidator::validate(QString& input, int& pos) const {
    input = input.trimmed();
    if (input.length() > 6) {
        input.remove(6, input.length() - 6);
        pos = std::min(pos, 6);
    }
    for (size_t i = 0; i < input.size(); i++) {
        if ((input[i] >= 'A' && input[i] <= 'Z') || (input[i] >= '0' && input[i] <= '9')) {
            continue;
        }
        if (input[i] >= 'a' && input[i] <= 'z') {
            input[i] = input[i].toUpper();
            continue;
        }
        return State::Invalid;
    }
    return State::Acceptable;
}

// void AccesstokenValidator::fixup(QString&) const {}

} // namespace

MainPage::MainPage(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MainPage) {

    ui->setupUi(parent);

    QAction* action1 = new QAction();
    action1->setIcon(QIcon(":/icons/icons/pc.png"));
    ui->device_id->addAction(action1, QLineEdit::LeadingPosition);

    QAction* action2 = new QAction();
    action2->setIcon(QIcon(":/icons/icons/lock.png"));
    ui->access_token->addAction(action2, QLineEdit::LeadingPosition);

    connect(ui->connect_btn, &QPushButton::pressed, [this]() { onConnectBtnPressed(); });
    ui->access_token->setValidator(new AccesstokenValidator(this));
}

MainPage::~MainPage() {
    delete ui;
}

void MainPage::onUpdateLocalDeviceID(int64_t device_id) {
    ui->my_device_id->setText(QString::fromStdString(std::to_string(device_id)));
}

void MainPage::onUpdateLocalAccessToken(const std::string& access_token) {
    ui->my_access_token->setText(QString::fromStdString(access_token));
}

void MainPage::onConnectBtnPressed() {
    auto dev_id = ui->device_id->text();
    auto token = ui->access_token->text();

    emit onConnectBtnPressed1(dev_id.toStdString(), token.toStdString());
}
