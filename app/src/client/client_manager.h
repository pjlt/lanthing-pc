#pragma once
#include <cstdint>

#include <functional>
#include <map>
#include <string>

#include <google/protobuf/message_lite.h>

#include <client/client_session.h>

namespace lt {

class ClientManager {
public:
    struct Params {
        std::function<void(const std::function<void()>&)> post_task;
        std::function<void(int64_t, const std::function<void()>&)> post_delay_task;
        std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>)> send_message;
        std::function<void(int64_t)> on_launch_client_success;
    };

public:
    ClientManager(const Params& params);
    void connect(int64_t peerDeviceID, const std::string& accessToken);
    void onRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg);

private:
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t, const std::function<void()>& task);
    void sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void tryRemoveSessionAfter10s(int64_t request_id);
    void tryRemoveSession(int64_t request_id);
    void onClientExited(int64_t request_id);

private:
    std::function<void(const std::function<void()>&)> post_task_;
    std::function<void(int64_t, const std::function<void()>&)> post_delay_task_;
    std::function<void(uint32_t, std::shared_ptr<google::protobuf::MessageLite>)> send_message_;
    std::function<void(int64_t)> on_launch_client_success_;
    std::atomic<int64_t> last_request_id_{0};
    std::map<int64_t /*request_id*/, std::shared_ptr<ClientSession>> sessions_;
    std::mutex session_mutex_;
};

} // namespace lt