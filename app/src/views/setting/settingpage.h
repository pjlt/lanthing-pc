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

#ifndef SETTINGPAGE_H
#define SETTINGPAGE_H

#include <optional>

#include <QValidator>
#include <QtWidgets/QWidget>

namespace Ui {
class SettingPage;
}

struct PreloadSettings {
    bool run_as_daemon;
    bool refresh_access_token;
    std::string relay_server;
    std::optional<bool> windowed_fullscreen;
};

class SettingPage : public QWidget {
    Q_OBJECT

public:
    explicit SettingPage(const PreloadSettings& preload, QWidget* parent = nullptr);
    ~SettingPage();

private:
    void onFullscreenModeToggle(bool is_windowed);

Q_SIGNALS:
    void runAsDaemonStateChanged(bool run_as_daemon);
    void refreshAccessTokenStateChanged(bool auto_refresh);
    void relayServerChanged(const std::string& svr);
    void fullscreenModeChanged(bool is_windowed);

private:
    Ui::SettingPage* ui;
    QRegularExpressionValidator* relay_validator_;
};

#endif // SETTINGPAGE_H
