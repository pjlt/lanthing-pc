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
#include <cstdint>

#include <functional>
#include <memory>
#include <optional>
#include <string>

#include <google/protobuf/message_lite.h>

#include <ltlib/settings.h>

namespace lt {

class GUIImpl;
class GUI {
public:
    enum class LoginStatus {
        Connected,
        Connecting,
        Disconnected,
    };

    enum class ServiceStatus {
        Up,
        Down,
    };

    enum class ConfirmResult { Accept, AcceptWithNextTime, Reject };

    enum class DeviceType { Gamepad, Mouse, Keyboard };

    struct Settings {
        bool run_as_daemon;
        bool auto_refresh_access_token;
        std::string relay_server;
        std::optional<bool> windowed_fullscreen;
        bool force_relay;
        uint16_t min_port;
        uint16_t max_port;
        std::optional<uint32_t> status_color;
    };

    struct TrustedDevice {
        int64_t device_id;
        bool gamepad;
        bool mouse;
        bool keyboard;
        int64_t last_access_time_s;
    };

    struct Params {
        std::function<void(int64_t, const std::string&)> connect;
        std::function<std::vector<std::string>()> get_history_device_ids;
        std::function<Settings()> get_settings;
        std::function<void(bool)> enable_auto_refresh_access_token;
        std::function<void(bool)> enable_run_as_service;
        std::function<void(bool /*is_windowed*/)> set_fullscreen_mode;
        std::function<void(const std::string&)> set_relay_server;
        std::function<void(int64_t, ConfirmResult)> on_user_confirmed_connection;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_operate_connection;
        std::function<void(int64_t, DeviceType, bool)> enable_device_permission;
        std::function<void(int64_t)> delete_trusted_device;
        std::function<std::vector<TrustedDevice>()> get_trusted_devices;
        std::function<void(bool)> force_relay;
        std::function<void(int64_t)> ignore_version;
        std::function<void(uint16_t, uint16_t)> set_port_range;
        std::function<void(int64_t)> set_status_color;
    };

public:
    GUI();

    void init(const Params& params, int argc, char** argv);

    int exec();

    void setDeviceID(int64_t device_id);

    void setAccessToken(const std::string& token);

    void setLoginStatus(LoginStatus statu);

    void onConfirmConnection(int64_t device_id);

    void onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onAccptedConnection(std::shared_ptr<google::protobuf::MessageLite> msg);

    void onDisconnectedConnection(int64_t device_id);

    void onServiceStatus(ServiceStatus status);

    void errorMessageBox(const std::string& message);

    void infoMessageBox(const std::string& message);

    void errorCode(int32_t code);

    void onNewVersion(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    std::shared_ptr<GUIImpl> impl_;
};

} // namespace lt