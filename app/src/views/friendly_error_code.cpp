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

#include "friendly_error_code.h"

#include <map>

#include <qobject.h>

#include <ltproto/error_code.pb.h>

QString errorCode2FriendlyMessage(int32_t code) {
    static const QString kPrefix = QObject::tr("Error code: %1");
    static const std::map<int32_t, QString> kCode2Msg = {
        // 0~ *******************************************
        {ltproto::ErrorCode::Success, QObject::tr("Success")},
        {ltproto::ErrorCode::Unknown, QObject::tr("Unknown")},
        {ltproto::ErrorCode::InternalError, QObject::tr("Internal error")},
        {ltproto::ErrorCode::InvalidParameter, QObject::tr("Invalid parameters")},
        {ltproto::ErrorCode::InvalidStatus,
         QObject::tr("Invalid status, the local program or server has invalid status")},
        {ltproto::ErrorCode::AppNotOnline,
         QObject::tr("Remote app not online, can't confirm connection")},
        {ltproto::ErrorCode::AuthFailed, QObject::tr("Auth failed")},
        {ltproto::ErrorCode::CreateServiceFailed, QObject::tr("Create service failed")},
        {ltproto::ErrorCode::StartServiceFailed, QObject::tr("Start service failed")},
        {ltproto::ErrorCode::ClientVresionTooLow, QObject::tr("Client version too low")},
        {ltproto::ErrorCode::HostVersionTooLow, QObject::tr("Host version too low")},
        {ltproto::ErrorCode::AccessCodeInvalid, QObject::tr("Access Code invalid")},
        // 10000~ *******************************************
        {ltproto::ErrorCode::DecodeFailed, QObject::tr("Decode failed")},
        {ltproto::ErrorCode::RenderFailed, QObject::tr("Render failed")},
        {ltproto::ErrorCode::NoDecodeAbility, QObject::tr("No decode ability")},
        {ltproto::ErrorCode::InitDecodeRenderPipelineFailed,
         QObject::tr("Initialize decode-render pipeline failed")},
        {ltproto::ErrorCode::WrokerInitVideoFailed,
         QObject::tr("Controlled side initialize video capture or video encoder failed")},
        {ltproto::ErrorCode::WorkerInitAudioFailed,
         QObject::tr("Controlled side initialize audio capture or audio encoder failed")},
        {ltproto::ErrorCode::WorkerInitInputFailed,
         QObject::tr("Controlled side initialize input executor failed")},
        {ltproto::ErrorCode::ControlledInitFailed,
         QObject::tr("Controlled side initialize failed")},
        {ltproto::ErrorCode::WorkerKeepAliveTimeout, QObject::tr("KeepAlive timeout")},
        {ltproto::ErrorCode::ServingAnotherClient, QObject::tr("Target is serving another client")},
        {ltproto::ErrorCode::TransportInitFailed, QObject::tr("Initialize transport failed")},
        {ltproto::ErrorCode::UserReject, QObject::tr("Peer user rejected you request")},
        // 30000~ *******************************************
        {ltproto::ErrorCode::AllocateDeviceIDNoAvailableID,
         QObject::tr("Request for allocating Device ID failed, server has no available ID, pleaese "
                     "contact the server owner to fix it")},
        {ltproto::ErrorCode::LoginDeviceInvalidID,
         QObject::tr("Login device failed, invalid device ID")},
        {ltproto::ErrorCode::LoginDeviceInvalidStatus,
         QObject::tr("Login device failed, server has invalid status")},
        // deprecated RequestConnectionClientRefuse
        {ltproto::ErrorCode::RequestConnectionClientRefuse,
         QObject::tr("Peer user rejected you request")},
        {ltproto::ErrorCode::RequestConnectionInvalidStatus,
         QObject::tr("Request connection failed, server has invalid status")},
        {ltproto::ErrorCode::RequestConnectionCreateOrderFailed,
         QObject::tr("Request connection failed, probably controlled side is "
                     "serving another clinet")},
        {ltproto::ErrorCode::RequestConnectionPeerNotOnline,
         QObject::tr("Request connection failed, peer not online")},
        {ltproto::ErrorCode::RequestConnectionTimeout, QObject::tr("Request connection timeout")},
        // 50000~ *******************************************
        {ltproto::ErrorCode::JoinRoomFailed,
         QObject::tr("Signaling server error, join room failed")},
        {ltproto::ErrorCode::SignalingPeerNotOnline,
         QObject::tr("Send signaling message failed, peer not online")},
        // 60000~ *******************************************
        {ltproto::ErrorCode::ServiceStatusDisconnectedFromServer,
         QObject::tr("Controlled module disconnected from server")},
        // 70000~ *******************************************
        {ltproto::ErrorCode::ClientStatusConnectTimeout, QObject::tr("Connect timeout")},
        {ltproto::ErrorCode::ClientStatusKeepAliveTimeout, QObject::tr("KeepAlive timeout")},
    };
    QString final_msg = kPrefix.arg(code);
    final_msg += "\n    ";
    auto iter = kCode2Msg.find(code);
    if (iter == kCode2Msg.end()) {
        iter = kCode2Msg.find(ltproto::ErrorCode::Unknown);
    }
    final_msg += iter->second;
    return final_msg;
}
