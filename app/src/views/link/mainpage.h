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

#ifndef MAINPAGE_H
#define MAINPAGE_H

#include <string>
#include <vector>

#include <QtWidgets/QWidget>
#include <QtWidgets/qlabel.h>
#include <google/protobuf/message_lite.h>

#include <transport/transport.h>

namespace Ui {
class MainPage;
}

namespace mdesk {
class App;
};

class MainPage : public QWidget {
    Q_OBJECT

public:
    explicit MainPage(const std::vector<std::string>& history_device_ids,
                      QWidget* parent = nullptr);
    ~MainPage();

    void onUpdateLocalDeviceID(int64_t device_id);

    void onUpdateLocalAccessToken(const std::string& access_token);

    void onClientStatus(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onAccptedClient(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onDisconnectedClient(int64_t device_id);

Q_SIGNALS:
    void onConnectBtnPressed1(const std::string& dev_id, const std::string& token);

private:
    void onConnectBtnPressed();

    void onUpdateIndicator();

    void loadPixmap();

    static void setPixmapForIndicator(bool enable, int64_t last_time, QLabel* label,
                                      const QPixmap& white, const QPixmap& gray, const QPixmap& red,
                                      const QPixmap& green);

private:
    Ui::MainPage* ui;
    std::vector<std::string> history_device_ids_;
    QPixmap mouse_white_;
    QPixmap mouse_gray_;
    QPixmap mouse_red_;
    QPixmap mouse_green_;
    QPixmap kb_white_;
    QPixmap kb_gray_;
    QPixmap kb_red_;
    QPixmap kb_green_;
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
    lt::VideoCodecType video_codec_ = lt::VideoCodecType::Unknown;
    std::optional<int64_t> peer_client_device_id_;
};

#endif // MAINPAGE_H
