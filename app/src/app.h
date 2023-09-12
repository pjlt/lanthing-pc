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
    struct Settings {
        bool run_as_daemon;
        bool auto_refresh_access_token;
        std::string relay_server;
    };

public:
    static std::unique_ptr<App> create();
    ~App();
    int exec(int argc, char** argv);
    void loginUser();
    void connect(int64_t deviceID, const std::string& accessToken);
    std::vector<std::string> getHistoryDeviceIDs() const;
    Settings getSettings() const;
    void enableRefreshAccessToken(bool enable);
    void enableRunAsDaemon(bool enable);
    void setRelayServer(const std::string& svr);

private:
    App();
    bool init();
    bool initSettings();
    void ioLoop(const std::function<void()>& i_am_alive);
    void tryRemoveSessionAfter10s(int64_t request_id);
    void tryRemoveSession(int64_t request_id);
    void onClientExitedThreadSafe(int64_t request_id);
    void createAndStartService();
    void stopService();
    void loadHistoryIDs();
    void saveHistoryIDs();
    void insertNewestHistoryID(const std::string& device_id);
    void maybeRefreshAccessToken();
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);

    // tcp client
    bool initTcpClient();
    void sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendMessageFromOtherThread(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
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
    std::mutex ioloop_mutex_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> tcp_client_;
    std::unique_ptr<ltlib::Settings> settings_;
    std::map<int64_t /*request_id*/, std::shared_ptr<ClientSession>> sessions_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    std::mutex session_mutex_;
    int64_t device_id_ = 0;
    std::string access_token_;
    std::vector<std::string> history_ids_;
    bool run_as_daemon_;
    bool auto_refresh_access_token_;
    std::string relay_server_;
    std::atomic<int64_t> last_request_id_{0};

    UiCallback* ui_;
};
} // namespace lt
