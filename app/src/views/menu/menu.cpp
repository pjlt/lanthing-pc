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

#include "menu.h"
#include "ui_menu.h"

QColor toColor(QString colorstr) {

    int r = colorstr.mid(1, 2).toInt(nullptr, 16);
    int g = colorstr.mid(3, 2).toInt(nullptr, 16);
    int b = colorstr.mid(5, 2).toInt(nullptr, 16);
    QColor color = QColor(r, g, b);
    return color;
}

Menu::Menu(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::Menu) {
    ui->setupUi(parent);

    connect(ui->main_page_btn, &QPushButton::pressed, [this]() { emit pageSelect(0); });
    connect(ui->settting_page_btn, &QPushButton::pressed, [this]() { emit pageSelect(1); });
    login_progress_ = new qt_componets::ProgressWidget();
    login_progress_->setVisible(false);
    login_progress_->setProgressColor(toColor("#8198ff"));
    ui->login_btn->setVisible(false);
    ui->login_status_layout->addStretch();
}

Menu::~Menu() {
    delete ui;
}

void Menu::setLoginStatus(LoginStatus status) {
    if (status == LoginStatus::LOGINING) {
        ui->login_status_layout->addWidget(login_progress_);
        login_progress_->setVisible(true);
        ui->info_label->setStyleSheet("QLabel{}");
    }
    else if (status == LoginStatus::LOGIN_SUCCESS) {
        ui->login_status_layout->removeWidget(login_progress_);
        login_progress_->setVisible(false);
        ui->info_label->setText("connected with server");
        ui->info_label->setStyleSheet("QLabel{}");
    }
    else if (status == LoginStatus::LOGIN_FAILED) {
        ui->login_status_layout->removeWidget(login_progress_);
        login_progress_->setVisible(false);
        ui->info_label->setText("disconnected with server");
        ui->info_label->setStyleSheet("QLabel{color: red}");
    }
}
