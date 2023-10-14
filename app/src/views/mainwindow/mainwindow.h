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

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <functional>

#include <QtWidgets/QMainWindow>

#include <views/gui.h>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class Menu;
class MainPage;
class SettingPage;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const lt::GUI::Params& params, QWidget* parent = nullptr);
    ~MainWindow();

    void switchToMainPage();

    void switchToSettingPage();
    void setLoginStatus(lt::GUI::ErrCode code);

    void setDeviceID(int64_t device_id);

    void setAccessToken(const std::string& access_token);

    void onConfirmConnection(int64_t device_id);

    void onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onDisconnectedConnection(int64_t device_id);

protected:
    void closeEvent(QCloseEvent* ev) override;

private:
    void doConnect(const std::string& dev_id, const std::string& token);

private:
    lt::GUI::Params params_;
    Ui::MainWindow* ui;

    Menu* menu_ui = nullptr;
    MainPage* main_page_ui;
    SettingPage* setting_page_ui;

    std::function<void()> switch_to_main_page_;
    std::function<void()> switch_to_setting_page_;
};
#endif // MAINWINDOW_H
