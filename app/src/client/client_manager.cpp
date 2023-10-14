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

#include "client_manager.h"

#include <ltproto/ltproto.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>

#include <ltlib/io/server.h>
#include <ltlib/logging.h>
#include <ltlib/system.h>

namespace {

constexpr ltproto::common::VideoCodecType kCodecPriority[] = {
    ltproto::common::VideoCodecType::HEVC,
    ltproto::common::VideoCodecType::AVC,
};

lt::VideoCodecType toLtrtc(ltproto::common::VideoCodecType codec) {
    switch (codec) {
    case ltproto::common::AVC:
        return lt::VideoCodecType::H264;
    case ltproto::common::HEVC:
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

std::unique_ptr<ClientManager> ClientManager::create(const Params& params) {
    std::unique_ptr<ClientManager> mgr{new ClientManager{params}};
    if (!mgr->init(params.ioloop)) {
        return nullptr;
    }
    return mgr;
}

bool ClientManager::init(ltlib::IOLoop* ioloop) {
    ltlib::Server::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop;
    params.pipe_name = "\\\\?\\pipe\\lanthing_client_manager";
    params.on_accepted = std::bind(&ClientManager::onPipeAccepted, this, std::placeholders::_1);
    params.on_closed = std::bind(&ClientManager::onPipeDisconnected, this, std::placeholders::_1);
    params.on_message = std::bind(&ClientManager::onPipeMessage, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    pipe_server_ = ltlib::Server::create(params);
    if (pipe_server_ == nullptr) {
        LOG(ERR) << "Init pipe server failed";
        return false;
    }
    return true;
}

void ClientManager::onPipeAccepted(uint32_t fd) {
    LOG(INFO) << "Local client accepted " << fd;
}

void ClientManager::onPipeDisconnected(uint32_t fd) {
    LOG(INFO) << "Local client disconnected " << fd;
}

void ClientManager::onPipeMessage(uint32_t fd, uint32_t type,
                                  std::shared_ptr<google::protobuf::MessageLite> msg) {
    (void)msg;
    LOGF(DEBUG, "Received local client %u msg %u", fd, type);
}

void ClientManager::connect(int64_t peerDeviceID, const std::string& accessToken,
                            const std::string& cookie) {
    // TODO: 先创建进程，从进程中获取一定信息才向服务器发请求
    int64_t request_id = last_request_id_.fetch_add(1);
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_request_id(request_id);
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(peerDeviceID);
    req->set_access_token(accessToken);
    req->set_cookie(cookie);
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
        using Backend = ltproto::common::StreamingParams::VideoEncodeBackend;
        using CodecType = ltproto::common::VideoCodecType;
        switch (codec) {
        case ltproto::common::AVC:
            if (h264_decodable) {
                params->add_video_codecs(CodecType::AVC);
            }
            break;
        case ltproto::common::HEVC:
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
    LOGF(INFO, "RequestConnection(device_id:%lld, request_id:%lld) sent", peerDeviceID, request_id);
    tryRemoveSessionAfter10s(request_id);
}

void ClientManager::onRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::RequestConnectionAck>(_msg);
    if (ack->err_code() != ltproto::ErrorCode::Success) {
        LOGF(WARNING, "RequestConnection(device_id:%" PRId64 ", request_id:%" PRId64 ") failed",
             ack->device_id(), ack->request_id());
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
    params.video_codec_type = toLtrtc(static_cast<ltproto::common::VideoCodecType>(
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
            LOGF(INFO,
                 "Received RequestConnectionAck(device_id:%" PRId64 ", request_id:%" PRId64
                 "), but too late",
                 ack->device_id(), ack->request_id());
            return;
        }
        else if (iter->second != nullptr) {
            LOGF(INFO,
                 "Received RequestConnectionAck(device_id:%" PRId64 ", request_id:%" PRId64
                 "), but another session "
                 "already started",
                 ack->device_id(), ack->request_id());
            return;
        }
        else {
            iter->second = session;
            LOGF(INFO,
                 "Received RequestConnectionAck(device_id:%" PRId64 ", request_id:%" PRId64 ")",
                 ack->device_id(), ack->request_id());
        }
    }
    if (!session->start()) {
        LOGF(INFO, "Start session(device_id:%" PRId64 ", request_id:%" PRId64 ") failed",
             ack->device_id(), ack->request_id());
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