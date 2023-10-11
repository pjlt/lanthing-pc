#include "client_manager.h"

#include <ltproto/ltproto.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>

#include <ltlib/logging.h>
#include <ltlib/system.h>

namespace {

constexpr ltproto::peer2peer::VideoCodecType kCodecPriority[] = {
    ltproto::peer2peer::VideoCodecType::HEVC,
    ltproto::peer2peer::VideoCodecType::AVC,
};

lt::VideoCodecType toLtrtc(ltproto::peer2peer::VideoCodecType codec) {
    switch (codec) {
    case ltproto::peer2peer::AVC:
        return lt::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return lt::VideoCodecType::H265;
    default:
        return lt::VideoCodecType::Unknown;
    }
}

} // namespace

namespace lt {

ClientManager::ClientManager(const Params& params)
    : post_task_{params.post_task}
    , post_delay_task_{params.post_delay_task}
    , send_message_{params.send_message}
    , on_launch_client_success_{params.on_launch_client_success} {}

void ClientManager::connect(int64_t peerDeviceID, const std::string& accessToken) {
    int64_t request_id = last_request_id_.fetch_add(1);
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_request_id(request_id);
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(peerDeviceID);
    req->set_access_token(accessToken);
    // HardDecodability abilities = lt::check_hard_decodability();
    bool h264_decodable = true;
    bool h265_decodable = true;
    ltlib::DisplayOutputDesc display_output_desc = ltlib::getDisplayOutputDesc();
    auto params = req->mutable_streaming_params();
    params->set_enable_driver_input(false);
    params->set_enable_gamepad(false);
    params->set_screen_refresh_rate(display_output_desc.frequency);
    params->set_video_width(display_output_desc.width);
    params->set_video_height(display_output_desc.height);
    for (auto codec : kCodecPriority) {
        using Backend = ltproto::peer2peer::StreamingParams::VideoEncodeBackend;
        using CodecType = ltproto::peer2peer::VideoCodecType;
        switch (codec) {
        case ltproto::peer2peer::AVC:
            if (h264_decodable) {
                params->add_video_codecs(CodecType::AVC);
            }
            break;
        case ltproto::peer2peer::HEVC:
            if (h265_decodable) {
                params->add_video_codecs(CodecType::HEVC);
            }
            break;
        default:
            break;
        }
    }
    if (params->video_codecs_size() == 0) {
        LOG(ERR) << "No decodability!";
        return;
    }
    {
        std::lock_guard<std::mutex> lock{session_mutex_};
        auto result = sessions_.insert({request_id, nullptr});
        if (!result.second) {
            LOG(ERR) << "Another task already connected/connecting to device_id:" << peerDeviceID;
            return;
        }
    }
    sendMessage(ltproto::id(req), req);
    LOGF(INFO, "RequestConnection(device_id:%d, request_id:%d) sent", peerDeviceID, request_id);
    tryRemoveSessionAfter10s(request_id);
}

void ClientManager::onRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::RequestConnectionAck>(_msg);
    if (ack->err_code() != ltproto::server::RequestConnectionAck_ErrCode_Success) {
        LOGF(WARNING, "RequestConnection(device_id:%d, request_id:%d) failed", ack->device_id(),
             ack->request_id());
        std::lock_guard<std::mutex> lock{session_mutex_};
        sessions_.erase(ack->request_id());
        return;
    }
    ClientSession::Params params{};
    params.client_id = ack->client_id();
    params.room_id = ack->room_id();
    params.auth_token = ack->auth_token();
    params.p2p_username = ack->p2p_username();
    params.p2p_password = ack->p2p_password();
    params.signaling_addr = ack->signaling_addr();
    params.signaling_port = ack->signaling_port();
    params.on_exited = std::bind(&ClientManager::onClientExited, this, ack->request_id());
    params.video_codec_type = toLtrtc(static_cast<ltproto::peer2peer::VideoCodecType>(
        ack->streaming_params().video_codecs().Get(0)));
    params.width = ack->streaming_params().video_width();
    params.height = ack->streaming_params().video_height();
    params.refresh_rate = ack->streaming_params().screen_refresh_rate();
    params.enable_driver_input = ack->streaming_params().enable_driver_input();
    params.enable_gamepad = ack->streaming_params().enable_gamepad();
    params.audio_channels = ack->streaming_params().audio_channels();
    params.audio_freq = ack->streaming_params().audio_sample_rate();
    for (int i = 0; i < ack->reflex_servers_size(); i++) {
        params.reflex_servers.push_back(ack->reflex_servers(i));
    }
    auto session = std::make_shared<ClientSession>(params);
    {
        std::lock_guard<std::mutex> lock{session_mutex_};
        auto iter = sessions_.find(ack->request_id());
        if (iter == sessions_.end()) {
            LOGF(INFO, "Received RequestConnectionAck(device_id:%d, request_id:%d), but too late",
                 ack->device_id(), ack->request_id());
            return;
        }
        else if (iter->second != nullptr) {
            LOGF(INFO,
                 "Received RequestConnectionAck(device_id:%d, request_id:%d), but another session "
                 "already started",
                 ack->device_id(), ack->request_id());
            return;
        }
        else {
            iter->second = session;
            LOGF(INFO, "Received RequestConnectionAck(device_id:, request_id:%d)", ack->device_id(),
                 ack->request_id());
        }
    }
    if (!session->start()) {
        LOGF(INFO, "Start session(device_id:%d, request_id:%d) failed", ack->device_id(),
             ack->request_id());
        std::lock_guard<std::mutex> lock{session_mutex_};
        sessions_.erase(ack->request_id());
    }
    on_launch_client_success_(ack->device_id());
}

void ClientManager::postTask(const std::function<void()>& task) {
    post_task_(task);
}

void ClientManager::postDelayTask(int64_t ms, const std::function<void()>& task) {
    post_delay_task_(ms, task);
}

void ClientManager::sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    send_message_(type, msg);
}

void ClientManager::tryRemoveSessionAfter10s(int64_t request_id) {
    postDelayTask(10'000, [request_id, this]() { tryRemoveSession(request_id); });
}

void ClientManager::tryRemoveSession(int64_t request_id) {
    std::lock_guard<std::mutex> lock{session_mutex_};
    auto iter = sessions_.find(request_id);
    if (iter == sessions_.end() || iter->second != nullptr) {
        return;
    }
    else {
        sessions_.erase(iter);
        LOG(WARNING) << "Remove session(request_id:" << request_id << ") by timeout";
    }
}

void ClientManager::onClientExited(int64_t request_id) {
    postTask([this, request_id]() {
        size_t size;
        {
            std::lock_guard<std::mutex> lock{session_mutex_};
            size = sessions_.erase(request_id);
        }
        if (size == 0) {
            LOG(WARNING)
                << "Try remove ClientSession due to client exited, but the session(request_id:"
                << request_id << ") doesn't exist.";
        }
        else {
            LOG(INFO) << "Remove session(request_id:" << request_id << ") success";
        }
    });
}

} // namespace lt