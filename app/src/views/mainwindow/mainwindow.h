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

#pragma once

#include <functional>

#include <QValidator>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>

#include <views/components/progress_widget.h>
#include <views/gui.h>

QT_BEGIN_NAMESPACE

class Ui_mainwindow;

QT_END_NAMESPACE

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(const lt::GUI::Params& params, QWidget* parent = nullptr);
    ~MainWindow();

    void switchToMainPage();

    void switchToSettingPage();

    void switchToManagerPage();

    void switchToAboutPage();

    void setLoginStatus(lt::GUI::LoginStatus status);

    void setServiceStatus(lt::GUI::ServiceStatus status);

    void setDeviceID(int64_t device_id);

    void setAccessToken(const std::string& access_token);

    void onConfirmConnection(int64_t device_id);

    void onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onDisconnectedConnection(int64_t device_id);

    void errorMessageBox(const QString& message);

    void infoMessageBox(const QString& message);

    void addOrUpdateTrustedDevice(int64_t device_id, int64_t time_s);

    void onNewVersion(std::shared_ptr<google::protobuf::MessageLite> msg);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override; // override?

private:
    void setupOtherCallbacks();

    void setLoginStatusInUIThread(lt::GUI::LoginStatus status);

    void setServiceStatusInUIThread(lt::GUI::ServiceStatus status);

    void setupClientIndicators();

    void loadPixmap();

    QPushButton* indexToTabButton(int32_t index);

    void swapTabBtnStyleSheet(QPushButton* old_selected, QPushButton* new_selected);

    void onConnectBtnClicked();

    void onShowTokenPressed();

    void onCopyPressed();

    void onUpdateIndicator();

    void onTimeoutHideToken();

    void addOrUpdateTrustedDevices();

    void addOrUpdateTrustedDevice(int64_t device_id, bool gamepad, bool mouse, bool keyboard,
                                  int64_t last_access_time);

    QWidget* makeWidgetHCentered(QWidget* widget);

    static void setPixmapForIndicator(bool enable, int64_t last_time, QLabel* label,
                                      const QPixmap& white, const QPixmap& gray, const QPixmap& red,
                                      const QPixmap& green);

private:
    lt::GUI::Params params_;
    std::string video_codec_ = "?";
    std::string access_token_text_;
    bool token_showing_ = false;
    int64_t token_last_show_time_ms_ = 0;
    std::optional<int64_t> peer_client_device_id_;
    std::vector<std::string> history_device_ids_;
    QPixmap copy_;
    QPixmap eye_close_;
    QPixmap eye_open_;
    QPixmap kick_;
    QPixmap mouse_;
    QPixmap mouse_white_;
    QPixmap mouse_gray_;
    QPixmap mouse_red_;
    QPixmap mouse_green_;
    QPixmap kb_;
    QPixmap kb_white_;
    QPixmap kb_gray_;
    QPixmap kb_red_;
    QPixmap kb_green_;
    QPixmap gp_;
    QPixmap gp_white_;
    QPixmap gp_gray_;
    QPixmap gp_red_;
    QPixmap gp_green_;
    int64_t mouse_hit_time_ = 0;
    int64_t keyboard_hit_time_ = 0;
    int64_t gamepad_hit_time_ = 0;
    bool enable_mouse_ = false;
    bool enable_keyboard_ = false;
    bool enable_gamepad_ = false;
    bool gpu_encode_ = false;
    bool gpu_decode_ = false;
    bool p2p_ = false;
    bool bandwidth_bps_ = 0;

    Ui_mainwindow* ui;
    QRegularExpressionValidator relay_validator_;
    QPointF old_pos_{};
    qt_componets::ProgressWidget* login_progress_ = nullptr;

    std::function<void()> switch_to_main_page_;
    std::function<void()> switch_to_setting_page_;
};
