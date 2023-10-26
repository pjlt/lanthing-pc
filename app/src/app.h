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

#include <cstddef>
#include <cstdint>

#include <random>

#include <QApplication>
#include <google/protobuf/message_lite.h>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/settings.h>
#include <ltlib/threads.h>

#include <client/client_manager.h>
#include <service/service_manager.h>
#include <views/gui.h>

namespace lt {

class App {
public:

public:
    static std::unique_ptr<App> create();
    ~App();
    int exec(int argc, char** argv);

private:
    App();
    bool init();
    bool initSettings();
    void ioLoop(const std::function<void()>& i_am_alive);

    void connect(int64_t deviceID, const std::string& accessToken);
    std::vector<std::string> getHistoryDeviceIDs() const;
    GUI::Settings getSettings() const;
    void enableRefreshAccessToken(bool enable);
    void enableRunAsDaemon(bool enable);
    void setRelayServer(const std::string& svr);
    void onUserConfirmedConnection(int64_t device_id, GUI::ConfirmResult result);
    void onOperateConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onFullscreenModeChanged(bool is_windowed);
    void enableDevicePermission(int64_t device_id, GUI::DeviceType type, bool enable);
    void deleteTrustedDevice(int64_t device_id);
    std::vector<GUI::TrustedDevice> getTrustedDevices();
    void setForceRelay(bool force);

    void createAndStartService();
    void stopService();
    void loadHistoryIDs();
    void saveHistoryIDs();
    void insertNewestHistoryID(const std::string& device_id);
    void maybeRefreshAccessToken();
    void onLaunchClientSuccess(int64_t device_id);
    void onConnectFailed(int64_t device_id, int32_t error_code);
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);

    // tcp client
    bool initTcpClient();
    void sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendMessageFromOtherThread(uint32_t type,
                                    std::shared_ptr<google::protobuf::MessageLite> msg);
    void onServerConnected();
    void onServerDisconnected();
    void onServerReconnecting();
    void onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void loginDevice();
    void allocateDeviceID();
    void sendKeepAlive();

    void handleAllocateDeviceIdAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void handleLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void handleRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> msg);

    // service
    bool initServiceManager();
    void onConfirmConnection(int64_t device_id);
    void onAccpetedConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onDisconnectedConnection(int64_t device_id);
    void onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg);

    //
    bool initClientManager();

    size_t rand();
    std::string generateAccessToken();

private:
    GUI gui_;
    std::mutex ioloop_mutex_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::unique_ptr<ltlib::Settings> settings_;
    std::unique_ptr<ClientManager> client_manager_;
    std::unique_ptr<ServiceManager> service_manager_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    int64_t device_id_ = 0;
    std::string access_token_;
    std::vector<std::string> history_ids_;
    bool run_as_daemon_ = false;
    bool auto_refresh_access_token_ = false;
    bool force_relay_ = false;
    std::optional<bool> windowed_fullscreen_;
    std::string relay_server_;
    std::mt19937 rand_engine_;
    std::uniform_int_distribution<size_t> rand_distrib_;
    bool signaling_keepalive_inited_ = false;
};
} // namespace lt
