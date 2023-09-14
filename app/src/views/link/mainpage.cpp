/*
 * BSD 3-Clause License
 * 
 * Copyright (c) 2023 Zhennan Tu <zhennan.tu@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *  
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

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
    void fixup(QString&) const override;
};

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

void AccesstokenValidator::fixup(QString& input) const {
    input = input.toUpper();
}

} // namespace

MainPage::MainPage(const std::vector<std::string>& history_device_ids, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::MainPage)
    , history_device_ids_(history_device_ids) {

    ui->setupUi(parent);

    QIcon pc_icon{":/icons/icons/pc.png"};
    if (history_device_ids_.empty()) {
        // ui->device_id->insertItem(0, "");
        // ui->device_id->setItemIcon(0, QIcon(":/icons/icons/pc.png"));
        ui->device_id->addItem(pc_icon, "");
    }
    else {
        for (const auto& id : history_device_ids_) {
            ui->device_id->addItem(pc_icon, QString::fromStdString(id));
        }
    }

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
    auto dev_id = ui->device_id->currentText();
    auto token = ui->access_token->text();

    emit onConnectBtnPressed1(dev_id.toStdString(), token.toStdString());
}
