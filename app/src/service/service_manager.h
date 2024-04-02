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
#include <functional>
#include <memory>

#include <ltlib/io/ioloop.h>
#include <ltlib/io/server.h>

#include <views/gui.h>

namespace lt {

//
class ServiceManager {
public:
    enum class ServiceStatus {
        Up,
        Down,
    };
    struct Params {
        ltlib::IOLoop* ioloop;
        std::function<void(int64_t)> on_confirm_connection;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_accepted_connection;
        std::function<void(int64_t)> on_disconnected_connection;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_connection_status;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_clipboard;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_pullfile;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_file_chunk;
        std::function<void(std::shared_ptr<google::protobuf::MessageLite>)>
            on_remote_file_chunk_ack;
        std::function<void(ServiceStatus)> on_service_status;
    };

public:
    static std::unique_ptr<ServiceManager> create(const Params& params);
    void onUserConfirmedConnection(int64_t device_id, GUI::ConfirmResult result);
    void onOperateConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void syncClipboardText(const std::string& text);
    void syncClipboardFile(int64_t my_device_id, uint32_t file_seq, const std::string& filename,
                           uint64_t size);
    void pullFileRequest(int64_t my_device_id, int64_t peer_device_id, uint32_t file_seq);
    void sendFileChunk(int64_t peer_device_id, uint32_t file_seq, uint32_t chunk_seq,
                       const uint8_t* data, uint16_t size);
    void sendFileChunkAck(int64_t peer_device_id, uint32_t file_seq, uint32_t chunk_seq);

private:
    ServiceManager(const Params& params);
    bool init(ltlib::IOLoop* ioloop);
    void onPipeAccepted(uint32_t fd);
    void onPipeDisconnected(uint32_t fd);
    void onPipeMessage(uint32_t fd, uint32_t type,
                       std::shared_ptr<google::protobuf::MessageLite> msg);
    void sendMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);

    void onConfirmConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onAcceptedConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onDisconnectedConnection(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onConnectionStatus(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onServiceStatus(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onRemoteClipboard(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onRemotePullFile(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onRemoteFileChunk(std::shared_ptr<google::protobuf::MessageLite> msg);
    void onRemoteFileChunkAck(std::shared_ptr<google::protobuf::MessageLite> msg);

private:
    std::unique_ptr<ltlib::Server> pipe_server_;
    uint32_t fd_ = std::numeric_limits<uint32_t>::max();
    std::function<void(int64_t)> on_confirm_connection_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_accepted_connection_;
    std::function<void(int64_t)> on_disconnected_connection_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_connection_status_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_clipboard_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_pullfile_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_file_chunk_;
    std::function<void(std::shared_ptr<google::protobuf::MessageLite>)> on_remote_file_chunk_ack_;
    std::function<void(ServiceStatus)> on_service_status_;
};
} // namespace lt