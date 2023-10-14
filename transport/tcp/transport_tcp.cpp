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

#include <transport/transport_tcp.h>

#include <WinSock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <ltlib/logging.h>

#include <ltproto/client2worker/audio_data.pb.h>
#include <ltproto/client2worker/video_frame.pb.h>
#include <ltproto/ltproto.h>

namespace {

const char* kKeyConnect = "connect";
const char* kKeyAddress = "address";

} // namespace

namespace lt {

namespace tp { // transport

bool ClientTCP::Params::validate() const {
    if (video_codec_type != VideoCodecType::H264 && video_codec_type != VideoCodecType::H265) {
        return false;
    }
    return !(on_data == nullptr || on_video == nullptr || on_audio == nullptr ||
             on_connected == nullptr || on_failed == nullptr || on_disconnected == nullptr ||
             on_signaling_message == nullptr);
}

std::unique_ptr<ClientTCP> ClientTCP::create(const Params& params) {
    if (!params.validate()) {
        return nullptr;
    }
    std::unique_ptr<ClientTCP> client{new ClientTCP{params}};
    if (!client->init()) {
        return nullptr;
    }
    return client;
}

ClientTCP::ClientTCP(const Params& params)
    : params_{params} {}

ClientTCP::~ClientTCP() {
    {
        std::lock_guard lock{mutex_};
        tcp_client_.reset();
        ioloop_.reset();
    }
}

bool ClientTCP::connect() {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ClientTCP::connect, this));
        return true;
    }
    params_.on_signaling_message(kKeyConnect, "");
    return true;
}

void ClientTCP::close() {}

bool ClientTCP::sendData(const uint8_t* data, uint32_t size, bool is_reliable) {
    if (!isNetworkThread()) {
        std::function<bool()> task = std::bind(&ClientTCP::sendData, this, data, size, is_reliable);
        return invoke(task);
    }
    // 已知data是[4_bytes_type|protobuf]结构，就偷懒不再套一层
    std::shared_ptr<uint8_t> _data{new uint8_t[size]};
    memcpy(_data.get(), data, size);
    return tcp_client_->send(_data, size);
}

void ClientTCP::onSignalingMessage(const char* _key, const char* _value) {
    std::string key = _key;
    std::string value = _value;
    onSignalingMessage2(key, value);
}

bool ClientTCP::init() {
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(ERR) << "Init ClientTCP IOLoop failed";
        return false;
    }
    task_thread_ = ltlib::TaskThread::create("ClientTCP_task");
    if (task_thread_ == nullptr) {
        return false;
    }
    return true;
}

bool ClientTCP::initTcpClient(const std::string& ip, uint16_t port) {
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = ip;
    params.port = port;
    params.is_tls = false;
    params.on_connected = std::bind(&ClientTCP::onConnected, this);
    params.on_closed = std::bind(&ClientTCP::onDisconnected, this);
    params.on_reconnecting = std::bind(&ClientTCP::onReconnecting, this);
    params.on_message =
        std::bind(&ClientTCP::onMessage, this, std::placeholders::_1, std::placeholders::_2);
    tcp_client_ = ltlib::Client::create(params);
    if (tcp_client_ == nullptr) {
        LOG(ERR) << "Init ClientTCP tcp client failed";
        return false;
    }
    net_thread_ = ltlib::BlockingThread::create(
        "ClientTCP_net", [this](const std::function<void()>& i_am_alive) { netLoop(i_am_alive); });
    return true;
}

bool ClientTCP::isNetworkThread() {
    return net_thread_->is_current_thread();
}

bool ClientTCP::isTaskThread() {
    return task_thread_->is_current_thread();
}

void ClientTCP::onConnected() {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ClientTCP::onConnected, this));
        return;
    }
    params_.on_connected();
}

void ClientTCP::onDisconnected() {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ClientTCP::onDisconnected, this));
        return;
    }
    params_.on_disconnected();
}

void ClientTCP::onReconnecting() {
    LOG(WARNING) << "ClientTCP reconnecting...";
}

void ClientTCP::onMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg) {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ClientTCP::onMessage, this, type, msg));
        return;
    }
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kVideoFrame:
    {
        auto frame = std::static_pointer_cast<ltproto::client2worker::VideoFrame>(msg);
        if (frame == nullptr) {
            LOG(WARNING) << "Cast MessageLite to VideoFrame failed";
            break;
        }
        lt::VideoFrame video_frame{};
        video_frame.is_keyframe = frame->is_keyframe();
        video_frame.ltframe_id = frame->picture_id();
        video_frame.data = reinterpret_cast<const uint8_t*>(frame->frame().c_str());
        video_frame.size = static_cast<uint32_t>(frame->frame().size());
        video_frame.width = frame->width();
        video_frame.height = frame->height();
        video_frame.capture_timestamp_us = frame->capture_timestamp_us();
        video_frame.start_encode_timestamp_us = frame->start_encode_timestamp_us();
        video_frame.end_encode_timestamp_us = frame->end_encode_timestamp_us();
        if (frame->has_temporal_id()) {
            video_frame.temporal_id = frame->temporal_id();
        }
        params_.on_video(video_frame);
        break;
    }
    case ltype::kAudioData:
    {
        auto audio_data = std::static_pointer_cast<ltproto::client2worker::AudioData>(msg);
        if (audio_data == nullptr) {
            LOG(WARNING) << "Cast MessageListe to AudioData failed";
            break;
        }
        lt::AudioData ad{};
        ad.data = audio_data->data().c_str();
        ad.size = static_cast<uint32_t>(audio_data->data().size());
        params_.on_audio(ad);
        break;
    }
    default:
    {
        // 写的时候偷懒，让运行的时候多绕一圈
        size_t size = msg->ByteSizeLong();
        if (size > 2 * 1024 * 1024) {
            LOG(ERR) << "ClientTCP received message too large(" << size << " bytes)";
            break;
        }
        std::vector<uint8_t> data(size + 4);
        *(uint32_t*)data.data() = type;
        if (!msg->SerializeToArray(data.data() + 4, static_cast<int>(size))) {
            LOG(ERR) << "ClientTCP serialize data failed, size " << size;
            break;
        }
        params_.on_data(data.data(), static_cast<uint32_t>(data.size()), true);
        break;
    }
    }
}

void ClientTCP::netLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "ClientTCP enter net loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "ClientTCP exit net loop";
}

void ClientTCP::onSignalingMessage2(const std::string& key, const std::string& value) {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ClientTCP::onSignalingMessage2, this, key, value));
        return;
    }
    if (key == kKeyAddress) {
        handleSigAddress(value);
    }
    else {
        LOG(WARNING) << "Unknown signaling message " << key;
    }
}

void ClientTCP::handleSigAddress(const std::string& value) {
    const auto pos = value.find(':');
    if (pos == std::string::npos || pos <= 0 || pos >= value.size() - 1) {
        return;
    }
    std::string ip_str = value.substr(0, pos);
    std::string port_str = value.substr(pos + 1);
    uint16_t port = static_cast<uint16_t>(std::atoi(port_str.c_str()));
    if (port == 0) {
        return;
    }
    LOGF(DEBUG, "value(%s), parsed(%s:%u)", value.c_str(), ip_str.c_str(), port);
    initTcpClient(ip_str, port);
}

void ClientTCP::invokeInternal(const std::function<void()>& task) {
    std::promise<void> promise;
    ioloop_->post([&promise, task]() {
        task();
        promise.set_value();
    });
    promise.get_future().get();
}

//*****************************************************************************

bool ServerTCP::Params::validate() const {
    if (video_codec_type != VideoCodecType::H264 && video_codec_type != VideoCodecType::H265) {
        return false;
    }
    return !(on_data == nullptr || on_accepted == nullptr || on_failed == nullptr ||
             on_disconnected == nullptr || on_signaling_message == nullptr);
}

std::unique_ptr<ServerTCP> ServerTCP::create(const Params& params) {
    if (!params.validate()) {
        return nullptr;
    }
    std::unique_ptr<ServerTCP> server{new ServerTCP{params}};
    if (!server->init()) {
        return nullptr;
    }
    return server;
}

ServerTCP::ServerTCP(const Params& params)
    : params_{params} {}

ServerTCP::~ServerTCP() {
    {
        std::lock_guard lock{mutex_};
        tcp_server_.reset();
        ioloop_.reset();
    }
}

void ServerTCP::close() {
    tcp_server_->close(client_fd_);
}

bool ServerTCP::sendData(const uint8_t* data, uint32_t size, bool is_reliable) {
    if (!isNetworkThread()) {
        std::function<bool()> task = std::bind(&ServerTCP::sendData, this, data, size, is_reliable);
        return invoke(task);
    }
    if (client_fd_ == std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    // 已知data是[4_bytes_type|protobuf]结构，就偷懒不再套一层
    std::shared_ptr<uint8_t> _data{new uint8_t[size]};
    memcpy(_data.get(), data, size);
    return tcp_server_->send(client_fd_, _data, size);
}

bool ServerTCP::sendAudio(const AudioData& audio_data) {
    if (!isNetworkThread()) {
        std::function<bool()> task = std::bind(&ServerTCP::sendAudio, this, audio_data);
        return invoke(task);
    }
    if (client_fd_ == std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    auto msg = std::make_shared<ltproto::client2worker::AudioData>();
    msg->set_data(audio_data.data, audio_data.size);
    return tcp_server_->send(client_fd_, ltproto::type::kAudioData, msg);
}

bool ServerTCP::sendVideo(const VideoFrame& frame) {
    if (!isNetworkThread()) {
        std::function<bool()> task = std::bind(&ServerTCP::sendVideo, this, frame);
        return invoke(task);
    }
    if (client_fd_ == std::numeric_limits<uint32_t>::max()) {
        return false;
    }
    auto video_frame = std::make_shared<ltproto::client2worker::VideoFrame>();
    video_frame->set_frame(frame.data, frame.size);
    video_frame->set_is_keyframe(frame.is_keyframe);
    video_frame->set_picture_id(frame.ltframe_id);
    video_frame->set_width(frame.width);
    video_frame->set_height(frame.height);
    video_frame->set_capture_timestamp_us(frame.capture_timestamp_us);
    video_frame->set_start_encode_timestamp_us(frame.start_encode_timestamp_us);
    video_frame->set_end_encode_timestamp_us(frame.end_encode_timestamp_us);
    if (frame.temporal_id.has_value()) {
        video_frame->set_temporal_id(frame.temporal_id.value());
    }
    return tcp_server_->send(client_fd_, ltproto::type::kVideoFrame, video_frame);
}

void ServerTCP::onSignalingMessage(const char* _key, const char* _value) {
    std::string key = _key;
    std::string value = _value;
    onSignalingMessage2(key, value);
}

bool ServerTCP::init() {
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(ERR) << "Init ServerTCP IOLoop failed";
        return false;
    }
    if (!initTcpServer()) {
        return false;
    }
    net_thread_ = ltlib::BlockingThread::create(
        "ServerTCP_net", [this](const std::function<void()>& i_am_alive) { netLoop(i_am_alive); });
    task_thread_ = ltlib::TaskThread::create("ServerTCP_task");
    if (task_thread_ == nullptr) {
        return false;
    }
    return true;
}

bool ServerTCP::initTcpServer() {
    ltlib::Server::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.bind_ip = "0.0.0.0";
    params.bind_port = 0;
    params.on_accepted = std::bind(&ServerTCP::onAccepted, this, std::placeholders::_1);
    params.on_closed = std::bind(&ServerTCP::onDisconnected, this, std::placeholders::_1);
    params.on_message = std::bind(&ServerTCP::onMessage, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    tcp_server_ = ltlib::Server::create(params);
    if (tcp_server_ == nullptr) {
        LOG(ERR) << "Init ServerTCP tcp server failed";
        return false;
    }
    return true;
}

bool ServerTCP::isNetworkThread() {
    return net_thread_->is_current_thread();
}

bool ServerTCP::isTaskThread() {
    return task_thread_->is_current_thread();
}

void ServerTCP::onAccepted(uint32_t fd) {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ServerTCP::onAccepted, this, fd));
        return;
    }
    if (client_fd_ != std::numeric_limits<uint32_t>::max()) {
        LOG(ERR) << "New ClientTCP(" << fd << ") connected to the ServerTCP, but another ClientTCP("
                 << fd << ") already being serve";
        tcp_server_->close(fd);
        return;
    }
    client_fd_ = fd;
    LOG(INFO) << "ServerTCP accpeted ClientTCP(" << fd << ")";
    params_.on_accepted();
}

void ServerTCP::onDisconnected(uint32_t fd) {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ServerTCP::onDisconnected, this, fd));
        return;
    }
    if (client_fd_ != fd) {
        LOG(FATAL) << "ClientTCP(" << fd << ") disconnected, but we are serving ClientTCP("
                   << client_fd_ << ")";
        return;
    }
    client_fd_ = std::numeric_limits<uint32_t>::max();
    LOGF(INFO, "ClientTCP(%d) disconnected from pipe server", fd);
    params_.on_disconnected();
}

void ServerTCP::onMessage(uint32_t fd, uint32_t type,
                          std::shared_ptr<google::protobuf::MessageLite> msg) {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ServerTCP::onMessage, this, fd, type, msg));
        return;
    }
    if (fd != client_fd_) {
        LOG(FATAL) << "fd != client_fd_";
        return;
    }
    // 写的时候偷懒，让运行的时候多绕一圈
    size_t size = msg->ByteSizeLong();
    if (size > 2 * 1024 * 1024) {
        LOG(ERR) << "ServerTCP received message too large(" << size << " bytes)";
        return;
    }
    std::vector<uint8_t> data(size + 4);
    *(uint32_t*)data.data() = type;
    if (!msg->SerializeToArray(data.data() + 4, static_cast<int>(size))) {
        LOG(ERR) << "ServerTCP serialize data failed, size " << size;
        return;
    }
    params_.on_data(data.data(), static_cast<uint32_t>(data.size()), true);
}

void ServerTCP::netLoop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "ServerTCP enter net loop";
    ioloop_->run(i_am_alive);
    LOG(INFO) << "ServerTCP exit net loop";
}

void ServerTCP::onSignalingMessage2(const std::string& key, const std::string& value) {
    if (!isTaskThread()) {
        task_thread_->post(std::bind(&ServerTCP::onSignalingMessage2, this, key, value));
        return;
    }
    if (key == kKeyConnect) {
        handleSigConnect();
    }
    else {
        LOG(WARNING) << "Unknown signaling message " << key;
    }
}

void ServerTCP::handleSigConnect() {
    // if (!initTcpServer()) {
    //     params_.on_failed();
    //     return;
    // }
    if (!gatherIP()) {
        params_.on_failed();
    }
}

// 仅取第一个非loopback的IPv4地址
bool ServerTCP::gatherIP() {
    ULONG flags = (GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST |
                   GAA_FLAG_INCLUDE_PREFIX);
    ULONG buffer_size = 16 * 16384;
    std::vector<uint8_t> buffer;
    PIP_ADAPTER_ADDRESSES adapters = nullptr;
    ULONG ret = 0;
    do {
        buffer.resize(buffer_size);
        adapters = reinterpret_cast<PIP_ADAPTER_ADDRESSES>(buffer.data());
        ret = GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &buffer_size);
    } while (ret == ERROR_BUFFER_OVERFLOW);
    if (ret != ERROR_SUCCESS) {
        return false;
    }
    while (adapters != nullptr) {
        if (adapters->OperStatus != IfOperStatusUp) {
            adapters = adapters->Next;
            continue;
        }
        if (adapters->IfType == IF_TYPE_SOFTWARE_LOOPBACK) {
            adapters = adapters->Next;
            continue;
        }
        PIP_ADAPTER_UNICAST_ADDRESS address = adapters->FirstUnicastAddress;
        if (address != nullptr) {
            sockaddr_in* v4_addr = reinterpret_cast<sockaddr_in*>(address->Address.lpSockaddr);
            char addr_buff[64] = {0};
            inet_ntop(v4_addr->sin_family, &v4_addr->sin_addr, addr_buff, 64);
            uint16_t port = tcp_server_->port();
            std::string value = std::string(addr_buff) + ":" + std::to_string(port);
            params_.on_signaling_message(kKeyAddress, value.c_str());
            return true;
        }
        adapters = adapters->Next;
    }
    return false;
}

void ServerTCP::invokeInternal(const std::function<void()>& task) {
    std::promise<void> promise;
    ioloop_->post([&promise, task]() {
        task();
        promise.set_value();
    });
    promise.get_future().get();
}

} // namespace tp

} // namespace lt