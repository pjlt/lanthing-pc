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

#include <filesystem>
#include <system_error>

#include <ltproto/client2app/client_status.pb.h>
#include <ltproto/common/clipboard.pb.h>
#include <ltproto/ltproto.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>

#include <ltlib/io/server.h>
#include <ltlib/logging.h>
#include <ltlib/system.h>
#include <ltlib/versions.h>

namespace {

lt::VideoCodecType toLtrtc(ltproto::common::VideoCodecType codec) {
    switch (codec) {
    case ltproto::common::AVC:
        return lt::VideoCodecType::H264_420;
    case ltproto::common::HEVC:
        return lt::VideoCodecType::H265_420;
    case ltproto::common::AVC_444:
        return lt::VideoCodecType::H264_444;
    case ltproto::common::HEVC_444:
        return lt::VideoCodecType::H265_444;
    case ltproto::common::AV1:
        return lt::VideoCodecType::AV1;
    case ltproto::common::AVC_SOFT:
        return lt::VideoCodecType::H264_420_SOFT;
    default:
        return lt::VideoCodecType::Unknown;
    }
}

} // namespace

namespace lt {

ClientManager::ClientManager(const Params& params)
    : decode_abilities_{params.decode_abilities}
    , codec_priority_{params.codec_priority}
    , post_task_{params.post_task}
    , post_delay_task_{params.post_delay_task}
    , send_message_{params.send_message}
    , on_launch_client_success_{params.on_launch_client_success}
    , on_connect_failed_{params.on_connect_failed}
    , on_client_status_{params.on_client_status}
    , on_remote_clipboard_{params.on_remote_clipboard}
    , close_connection_{params.close_connection} {}

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
#if LT_WINDOWS
    params.pipe_name = "\\\\?\\pipe\\lanthing_client_manager";
#elif LT_LINUX
    std::filesystem::path fs = ltlib::getConfigPath();
    fs = fs / "pipe_lanthing_client_manager";
    params.pipe_name = fs.string();
    // 上次进程崩溃可能会导致残留管道文件
    std::error_code ec;
    std::filesystem::remove(fs, ec);
#else
#endif
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
    fd_ = fd;
}

void ClientManager::onPipeDisconnected(uint32_t fd) {
    LOG(INFO) << "Local client disconnected " << fd;
    fd_ = std::numeric_limits<uint32_t>::max();
}

void ClientManager::onPipeMessage(uint32_t fd, uint32_t type,
                                  std::shared_ptr<google::protobuf::MessageLite> msg) {
    (void)fd;
    switch (type) {
    case ltproto::type::kClientStatus:
        onClientStatus(msg);
        break;
    case ltproto::type::kClipboard:
        onRemoteClipboard(msg);
        break;
    default:
        break;
    }
}

// 跑在IOLoop线程
void ClientManager::connect(int64_t peerDeviceID, const std::string& accessToken,
                            const std::string& cookie, bool use_tcp) {
    // TODO: 先创建进程，从进程中获取一定信息才向服务器发请求
    int64_t request_id = last_request_id_.fetch_add(1);
    auto req = std::make_shared<ltproto::server::RequestConnection>();
    req->set_transport_type(use_tcp ? ltproto::common::TransportType::TCP
                                    : ltproto::common::TransportType::RTC);
    req->set_request_id(request_id);
    req->set_conn_type(ltproto::server::ConnectionType::Control);
    req->set_device_id(peerDeviceID);
    req->set_access_token(accessToken);
    req->set_cookie(cookie);
    req->set_client_version(
        ltlib::combineVersion(LT_VERSION_MAJOR, LT_VERSION_MINOR, LT_VERSION_PATCH));
    req->set_required_version(ltlib::combineVersion(0, 3, 3));
    ltlib::DisplayOutputDesc display_output_desc = ltlib::getDisplayOutputDesc("");
    auto params = req->mutable_streaming_params();
    params->set_enable_driver_input(false);
    params->set_enable_gamepad(false);
    params->set_screen_refresh_rate(display_output_desc.frequency);
    params->set_video_width(display_output_desc.width);
    params->set_video_height(display_output_desc.height);
    for (auto codec : codec_priority_) {
        using CodecType = ltproto::common::VideoCodecType;
        switch (codec) {
        case VideoCodecType::H264_420:
            if (decode_abilities_ & VideoCodecType::H264_420) {
                params->add_video_codecs(CodecType::AVC);
            }
            break;
        case VideoCodecType::H265_420:
            if (decode_abilities_ & VideoCodecType::H265_420) {
                params->add_video_codecs(CodecType::HEVC);
            }
            break;
        case VideoCodecType::H264_444:
            if (decode_abilities_ & VideoCodecType::H264_444) {
                params->add_video_codecs(CodecType::AVC_444);
            }
            break;
        case VideoCodecType::H265_444:
            if (decode_abilities_ & VideoCodecType::H265_444) {
                params->add_video_codecs(CodecType::HEVC_444);
            }
            break;
        case VideoCodecType::AV1:
            if (decode_abilities_ & VideoCodecType::AV1) {
                params->add_video_codecs(CodecType::AV1);
            }
            break;
        case VideoCodecType::H264_420_SOFT:
            if (decode_abilities_ & VideoCodecType::H264_420_SOFT) {
                params->add_video_codecs(CodecType::AVC_SOFT);
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

    auto result = sessions_.insert({request_id, nullptr});
    if (!result.second) {
        LOG(ERR) << "Another task already connected/connecting to device_id:" << peerDeviceID;
        return;
    }

    sendMessage(ltproto::id(req), req);
    LOGF(INFO, "RequestConnection(device_id:%" PRId64 ", request_id:%" PRId64 ") sent",
         peerDeviceID, request_id);
    tryRemoveSessionAfter10s(request_id);
}

// 跑在IOLoop线程
void ClientManager::onRequestConnectionAck(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto ack = std::static_pointer_cast<ltproto::server::RequestConnectionAck>(_msg);
    if (ack->err_code() != ltproto::ErrorCode::Success) {
        LOGF(WARNING,
             "RequestConnection(device_id:%" PRId64 ", request_id:%" PRId64
             ") failed, error code: %d %s",
             ack->device_id(), ack->request_id(), static_cast<int32_t>(ack->err_code()),
             ltproto::ErrorCode_Name(ack->err_code()).c_str());
        sessions_.erase(ack->request_id());
        on_connect_failed_(ack->device_id(), ack->err_code());
        return;
    }
    ClientSession::Params params{};
    params.transport_type = ack->transport_type();
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
    params.rotation = ack->streaming_params().rotation();
    for (int i = 0; i < ack->reflex_servers_size(); i++) {
        params.reflex_servers.push_back(ack->reflex_servers(i));
    }
    auto session = std::make_shared<ClientSession>(params);

    auto iter = sessions_.find(ack->request_id());
    if (iter == sessions_.end()) {
        LOGF(INFO,
             "Received RequestConnectionAck(device_id:%" PRId64 ", request_id:%" PRId64
             "), but too late",
             ack->device_id(), ack->request_id());
        // 太晚了，通知服务器关闭订单
        close_connection_(ack->room_id());
        return;
    }
    else if (iter->second != nullptr) {
        LOGF(ERR,
             "Received RequestConnectionAck(device_id:%" PRId64 ", request_id:%" PRId64
             "), but another session "
             "already started",
             ack->device_id(), ack->request_id());
        // 同样的request_id，这是写bug了。应不应该通知服务器关闭订单？
        close_connection_(ack->room_id());
        return;
    }
    else {
        iter->second = session;
        LOGF(INFO, "Received RequestConnectionAck(device_id:%" PRId64 ", request_id:%" PRId64 ")",
             ack->device_id(), ack->request_id());
    }

    if (!session->start()) {
        LOGF(INFO, "Start session(device_id:%" PRId64 ", request_id:%" PRId64 ") failed",
             ack->device_id(), ack->request_id());
        sessions_.erase(ack->request_id());
        // 启动失败，通知服务器关闭订单
        close_connection_(ack->room_id());
        return;
    }
    on_launch_client_success_(ack->device_id());
}

void ClientManager::syncClipboardText(const std::string& text) {
    auto msg = std::make_shared<ltproto::common::Clipboard>();
    msg->set_type(ltproto::common::Clipboard_ClipboardType_Text);
    msg->set_text(text);
    pipe_server_->send(fd_, ltproto::id(msg), msg);
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

// 这里有个小问题，如果被控10秒内不点“同意”，就没了
void ClientManager::tryRemoveSessionAfter10s(int64_t request_id) {
    postDelayTask(10'000, [request_id, this]() { tryRemoveSession(request_id); });
}

// 跑在IOLoop线程
void ClientManager::tryRemoveSession(int64_t request_id) {
    auto iter = sessions_.find(request_id);
    if (iter == sessions_.end() || iter->second != nullptr) {
        return;
    }
    else {
        sessions_.erase(iter);
        LOG(WARNING) << "Remove session(request_id:" << request_id << ") by timeout";
        on_connect_failed_(0, ltproto::ErrorCode::RequestConnectionTimeout);
    }
}

// ClientSession线程 -> IOLoop线程
void ClientManager::onClientExited(int64_t request_id) {
    postTask([this, request_id]() {
        auto iter = sessions_.find(request_id);
        if (iter == sessions_.end()) {
            LOG(WARNING)
                << "Try remove ClientSession due to client exited, but the session(request_id:"
                << request_id << ") doesn't exist.";
        }
        else {
            std::string room_id = iter->second->roomID();
            sessions_.erase(iter);
            LOG(INFO) << "Remove session(request_id:" << request_id << ", room_id: " << room_id
                      << ") success";
            close_connection_(room_id);
        }
    });
}

void ClientManager::onClientStatus(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::client2app::ClientStatus>(_msg);
    on_client_status_(msg->status());
}

void ClientManager::onRemoteClipboard(std::shared_ptr<google::protobuf::MessageLite> msg) {
    on_remote_clipboard_(msg);
}

} // namespace lt