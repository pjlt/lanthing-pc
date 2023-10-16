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

#include "settingpage.h"
#include "ui_settingpage.h"

#include <g3log/g3log.hpp>

SettingPage::SettingPage(const PreloadSettings& preload, QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::SettingPage)
    , relay_validator_(new QRegularExpressionValidator(
          QRegularExpression("relay:(.+?:[0-9]+?):(.+?):(.+?)"), this)) {
    ui->setupUi(parent);
    ui->checkbox_daemon->setChecked(preload.run_as_daemon);
    ui->checkbox_refresh_passwd->setChecked(preload.refresh_access_token);
    ui->lineedit_relay->setText(QString::fromStdString(preload.relay_server));
    ui->button_relay->setEnabled(false);

    if (preload.windowed_fullscreen.has_value()) {
        ui->radio_windowed_fullscreen->setChecked(preload.windowed_fullscreen.value());
        ui->radio_fullscreen->setChecked(!preload.windowed_fullscreen.value());
    }
    else {
        ui->radio_windowed_fullscreen->setChecked(false);
        ui->radio_fullscreen->setChecked(false);
    }

    connect(ui->checkbox_daemon, &QCheckBox::stateChanged,
            [this](int) { emit runAsDaemonStateChanged(ui->checkbox_daemon->isChecked()); });
    connect(ui->checkbox_refresh_passwd, &QCheckBox::stateChanged, [this](int) {
        emit refreshAccessTokenStateChanged(ui->checkbox_refresh_passwd->isChecked());
    });

    connect(ui->radio_windowed_fullscreen, &QRadioButton::toggled, this,
            &SettingPage::onFullscreenModeToggle);

    connect(ui->lineedit_relay, &QLineEdit::textChanged, [this](const QString& _text) {
        if (_text.isEmpty()) {
            ui->button_relay->setEnabled(true);
            return;
        }
        QString text = _text;
        int pos = text.length(); // -1; ???
        QValidator::State state = relay_validator_->validate(text, pos);
        ui->button_relay->setEnabled(state == QValidator::State::Acceptable);
    });
    connect(ui->button_relay, &QPushButton::released, [this]() {
        ui->button_relay->setEnabled(false);
        emit relayServerChanged(ui->lineedit_relay->text().toStdString());
    });
}

SettingPage::~SettingPage() {
    delete ui;
    delete relay_validator_;
}

void SettingPage::onFullscreenModeToggle(bool is_windowed) {
    emit fullscreenModeChanged(is_windowed);
}
