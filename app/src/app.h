#pragma once

#include <cstddef>
#include <cstdint>

#include <google/protobuf/message_lite.h>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/settings.h>
#include <ltlib/threads.h>

#include "client_session.h"

#include "views/mainwindow.h"
#include <QApplication>

namespace lt {

class App {
public:
    static std::unique_ptr<App> create();
    ~App();
    int exec(int argc, char** argv);
    void loginUser();
    void connect(int64_t deviceID, const std::string& accessToken);
    std::vector<std::string> getHistoryDeviceIDs();

private:
    App();
    bool init();
    bool initSettings();
    void ioLoop(const std::function<void()>& i_am_alive);
    void tryRemoveSessionAfter10s(int64_t device_id);
    void tryRemoveSession(int64_t device_id);
    void onClientExitedThreadSafe(int64_t device_id);
    void createAndStartService();
    void stopService();

    // tcp client
    bool initTcpClient();
    void sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void onServerConnected();
    void onServerDisconnected();
    void onServerReconnecting();
    void onServerMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void loginDevice();
    void allocateDeviceID();

    // message handler
    void handleAllocateDeviceIdAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void handleLoginDeviceAck(std::shared_ptr<google::protobuf::MessageLite> msg);
    void handleRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::unique_ptr<ltlib::Settings> settings_;
    std::map<int64_t /*to_device*/, std::shared_ptr<ClientSession>> sessions_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::mutex mutex_;
    int64_t device_id_ = 0;
    std::string access_token_;

    UiCallback* ui_;
};
} // namespace lt
