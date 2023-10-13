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

#include "service_manager.h"

#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/confirm_connection.pb.h>
#include <ltproto/peer2peer/confirm_connection_ack.pb.h>

#include <ltlib/logging.h>

namespace lt {

std::unique_ptr<ServiceManager> ServiceManager::create(const Params& params) {
    std::unique_ptr<ServiceManager> mgr{new ServiceManager{params}};
    if (!mgr->init(params.ioloop)) {
        return nullptr;
    }
    return mgr;
}

ServiceManager::ServiceManager(const Params& params)
    : on_confirm_connection_{params.on_confirm_connection} {}

bool ServiceManager::init(ltlib::IOLoop* ioloop) {
    ltlib::Server::Params params{};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop;
    params.pipe_name = "\\\\?\\pipe\\lanthing_service_manager";
    params.on_accepted = std::bind(&ServiceManager::onPipeAccepted, this, std::placeholders::_1);
    params.on_closed = std::bind(&ServiceManager::onPipeDisconnected, this, std::placeholders::_1);
    params.on_message = std::bind(&ServiceManager::onPipeMessage, this, std::placeholders::_1,
                                  std::placeholders::_2, std::placeholders::_3);
    pipe_server_ = ltlib::Server::create(params);
    if (pipe_server_ == nullptr) {
        LOG(ERR) << "Init pipe server failed";
        return false;
    }
    return true;
}

void ServiceManager::onPipeAccepted(uint32_t fd) {
    LOG(INFO) << "Service accepted " << fd;
    fd_ = fd;
}

void ServiceManager::onPipeDisconnected(uint32_t fd) {
    LOG(INFO) << "Service disconnected " << fd;
    fd_ = std::numeric_limits<uint32_t>::max();
}

void ServiceManager::onPipeMessage(uint32_t fd, uint32_t type,
                                   std::shared_ptr<google::protobuf::MessageLite> msg) {
    (void)msg;
    LOGF(DEBUG, "Received service %u msg %u", fd, type);
    switch (type) {
    case ltproto::type::kConfirmConnection:
        onConfirmConnection(msg);
        break;
    default:
        LOG(WARNING) << "ServiceManager received unknown messge type " << type;
        break;
    }
}

void ServiceManager::onConfirmConnection(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::ConfirmConnection>(_msg);
    on_confirm_connection_(msg->device_id());
}

void ServiceManager::onUserConfirmedConnection(int64_t device_id, GUI::ConfirmResult result) {
    auto ack = std::make_shared<ltproto::peer2peer::ConfirmConnectionAck>();
    ack->set_device_id(device_id);
    switch (result) {
    case GUI::ConfirmResult::Accept:
        ack->set_result(ltproto::peer2peer::ConfirmConnectionAck_ConfirmResult_Agree);
        break;
    case GUI::ConfirmResult::AcceptWithNextTime:
        ack->set_result(ltproto::peer2peer::ConfirmConnectionAck_ConfirmResult_AgreeNextTime);
        break;
    case GUI::ConfirmResult::Reject:
        [[fallthrough]];
    default:
        ack->set_result(ltproto::peer2peer::ConfirmConnectionAck_ConfirmResult_Reject);
        break;
    }
    pipe_server_->send(fd_, ltproto::id(ack), ack);
}

} // namespace lt