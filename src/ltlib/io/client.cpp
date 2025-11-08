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

#include "client_secure_layer.h"
#include "client_transport_layer.h"
#include "picohttpparser.h"
#include <ltlib/io/client.h>
#include <ltlib/logging.h>
#include <ltlib/strings.h>
#include <ltproto/ltproto.h>

namespace {

template <typename T>
    requires std::is_integral_v<T>
T randomIntegral(T min = std::numeric_limits<T>::min(), T max = std::numeric_limits<T>::max()) {
    static std::random_device rd;
    static std::mt19937 engine(rd());
    static std::uniform_int_distribution<T> distrib(min, max);
    return distrib(engine);
}

} // namespace

namespace ltlib {

class IClientImpl {
public:
    virtual ~IClientImpl() {};
    virtual bool init() = 0;
    virtual bool send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
                      const std::function<void()>& callback) = 0;
    virtual bool send(const std::shared_ptr<uint8_t>& data, uint32_t len,
                      const std::function<void()>& callback) = 0;
    virtual void reconnect() = 0;
};

class ClientImpl : public IClientImpl {
public:
    ClientImpl(const Client::Params& params);
    ~ClientImpl() override;
    bool init() override;
    bool send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
              const std::function<void()>& callback) override;
    bool send(const std::shared_ptr<uint8_t>& data, uint32_t len,
              const std::function<void()>& callback) override;
    void reconnect() override;

private:
    CTransport::Params make_transport_params(const Client::Params& cparams);
    bool on_transport_connected();
    void on_transport_closed();
    void on_transport_reconnecting();
    bool on_transport_read(const Buffer& buff);

private:
    bool connected_ = false;
    IOLoop* ioloop_;
    std::function<void()> on_connected_;
    std::function<void()> on_closed_;
    std::function<void()> on_reconnecting_;
    std::function<void(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>&)>
        on_message_;
    std::unique_ptr<CTransport> transport_;
    ltproto::Parser parser_;
};

ClientImpl::ClientImpl(const Client::Params& params)
    : ioloop_{params.ioloop}
    , on_connected_{params.on_connected}
    , on_closed_{params.on_closed}
    , on_reconnecting_{params.on_reconnecting}
    , on_message_{params.on_message} {
    if (params.is_tls) {
        transport_ = std::make_unique<MbedtlsCTransport>(make_transport_params(params));
    }
    else {
        transport_ = std::make_unique<LibuvCTransport>(make_transport_params(params));
    }
}

ClientImpl::~ClientImpl() {
    //
}

CTransport::Params ClientImpl::make_transport_params(const Client::Params& cparams) {
    CTransport::Params tparams{};
    tparams.stype = cparams.stype;
    tparams.ioloop = cparams.ioloop;
    tparams.pipe_name = cparams.pipe_name;
    tparams.host = cparams.host;
    tparams.port = cparams.port;
    tparams.cert = cparams.cert;
    tparams.on_connected = std::bind(&ClientImpl::on_transport_connected, this);
    tparams.on_closed = std::bind(&ClientImpl::on_transport_closed, this);
    tparams.on_reconnecting = std::bind(&ClientImpl::on_transport_reconnecting, this);
    tparams.on_read = std::bind(&ClientImpl::on_transport_read, this, std::placeholders::_1);
    return tparams;
}

bool ClientImpl::init() {
    return transport_->init();
}

bool ClientImpl::on_transport_connected() {
    connected_ = true;
    on_connected_();
    return true;
}

void ClientImpl::on_transport_closed() {
    connected_ = false;
    on_closed_();
}

void ClientImpl::on_transport_reconnecting() {
    connected_ = false;
    parser_.clear();
    on_reconnecting_();
}

bool ClientImpl::on_transport_read(const Buffer& buff) {
    parser_.push_buffer(reinterpret_cast<const uint8_t*>(buff.base), buff.len);
    if (!parser_.parse_buffer()) {
        return false;
    }
    while (auto msg = parser_.pop_message()) {
        on_message_(msg->type, msg->msg);
    }
    return true;
}

bool ClientImpl::send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
                      const std::function<void()>& callback) {
    if (!ioloop_->isCurrentThread()) {
        LOG(FATAL) << "Send data in wrong thread!";
        return false;
    }
    if (!connected_) {
        return false;
    }
    auto packet = ltproto::Packet::create({type, msg}, true);
    if (!packet.has_value()) {
        LOG(ERR) << "Create net packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    Buffer buff[2] = {{(char*)&pkt.header, sizeof(pkt.header)},
                      {(char*)pkt.payload.get(), pkt.header.payload_size}};
    return transport_->send(buff, 2, [packet, callback]() {
        // 把packet capture进来，是为了延续内部shared_ptr的生命周期
        if (callback != nullptr) {
            callback();
        }
    });
}

bool ClientImpl::send(const std::shared_ptr<uint8_t>& data, uint32_t len,
                      const std::function<void()>& callback) {
    if (!ioloop_->isCurrentThread()) {
        LOG(FATAL) << "Send data in wrong thread!";
        return false;
    }
    if (!connected_) {
        return false;
    }
    auto packet = ltproto::Packet::create(data, len, true);
    if (!packet.has_value()) {
        LOG(ERR) << "Create net packet failed";
        return false;
    }
    const auto& pkt = packet.value();
    Buffer buff[2] = {{(char*)&pkt.header, sizeof(pkt.header)},
                      {(char*)pkt.payload.get(), pkt.header.payload_size}};
    return transport_->send(buff, 2, [packet, callback]() {
        // 把packet capture进来，是为了延续内部shared_ptr的生命周期
        if (callback != nullptr) {
            callback();
        }
    });
}

void ClientImpl::reconnect() {
    transport_->reconnect();
}

class WSClientImpl : public IClientImpl {
    // https://github.com/dhbaird/easywsclient
    enum class WSState { Closed, TcpConnecting, SentHttp, WSConnected };
    struct wsheader_type {
        unsigned header_size;
        bool fin;
        bool mask;
        enum opcode_type : uint8_t {
            CONTINUATION = 0x0,
            TEXT_FRAME = 0x1,
            BINARY_FRAME = 0x2,
            CLOSE = 8,
            PING = 9,
            PONG = 0xa,
        } opcode;
        int N0;
        uint64_t N;
        uint8_t masking_key[4];
    };
    struct Packet {
        Packet()
            : header{new uint8_t[14]} {}
        std::shared_ptr<uint8_t> header;
        uint32_t header_length = 0;
        std::shared_ptr<uint8_t> payload;
        uint32_t payload_length = 0;
    };

public:
    WSClientImpl(const Client::Params& params);
    ~WSClientImpl() override;
    bool init() override;
    bool send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
              const std::function<void()>& callback) override;
    bool send(const std::shared_ptr<uint8_t>& data, uint32_t len,
              const std::function<void()>& callback) override;
    void reconnect() override;

private:
    CTransport::Params make_transport_params(const Client::Params& cparams);
    bool on_transport_connected();
    void on_transport_closed();
    void on_transport_reconnecting();
    bool on_transport_read(const Buffer& buff);
    bool parse_http_response();
    bool parse_ws();
    bool on_ws_message();
    bool send_ws_message(wsheader_type::opcode_type type, const std::shared_ptr<uint8_t>& data,
                         uint32_t len, const std::function<void()>& callback);

private:
    bool connected_ = false;
    IOLoop* ioloop_;
    std::function<void()> on_connected_;
    std::function<void()> on_closed_;
    std::function<void()> on_reconnecting_;
    std::function<void(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>&)>
        on_message_;
    std::unique_ptr<CTransport> transport_;
    // ltproto::Parser parser_;
    std::string path_;
    std::string host_;
    uint16_t port_;
    std::string rand16_;
    std::string rand16_base64_;
    WSState state_ = WSState::Closed;
    std::vector<uint8_t> buffer_;
    std::vector<uint8_t> current_msg_;
    wsheader_type::opcode_type current_opcode_;
    bool use_mask_ = false;
};

WSClientImpl::WSClientImpl(const Client::Params& params)
    : ioloop_{params.ioloop}
    , on_connected_{params.on_connected}
    , on_closed_{params.on_closed}
    , on_reconnecting_{params.on_reconnecting}
    , on_message_{params.on_message} {
    if (params.is_tls) {
        transport_ = std::make_unique<MbedtlsCTransport>(make_transport_params(params));
    }
    else {
        transport_ = std::make_unique<LibuvCTransport>(make_transport_params(params));
    }
}

WSClientImpl::~WSClientImpl() {
    //
}

CTransport::Params WSClientImpl::make_transport_params(const Client::Params& cparams) {
    CTransport::Params tparams{};
    tparams.stype = StreamType::TCP;
    tparams.ioloop = cparams.ioloop;
    tparams.host = cparams.host;
    tparams.port = cparams.port;
    tparams.cert = cparams.cert;
    tparams.on_connected = std::bind(&WSClientImpl::on_transport_connected, this);
    tparams.on_closed = std::bind(&WSClientImpl::on_transport_closed, this);
    tparams.on_reconnecting = std::bind(&WSClientImpl::on_transport_reconnecting, this);
    tparams.on_read = std::bind(&WSClientImpl::on_transport_read, this, std::placeholders::_1);
    return tparams;
}

bool WSClientImpl::init() {
    // TODO: 解析url
    rand16_ = randomStr(16);
    rand16_base64_ = base64Encode(rand16_);
    return transport_->init();
}

bool WSClientImpl::send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
                        const std::function<void()>& callback) {
    size_t length = msg->ByteSizeLong();
    std::shared_ptr<uint8_t> payload = std::shared_ptr<uint8_t>(new uint8_t[length + 4]);
    *reinterpret_cast<uint32_t*>(payload.get()) = type;
    if (!msg->SerializeToArray(payload.get() + 4, static_cast<int>(length) - 4)) {
        LOG(ERR) << "Serialize protobuf message failed, type:" << type;
        return false;
    }

    return send_ws_message(wsheader_type::opcode_type::BINARY_FRAME, payload,
                           static_cast<uint32_t>(length + 4), callback);
}

bool WSClientImpl::send(const std::shared_ptr<uint8_t>& data, uint32_t len,
                        const std::function<void()>& callback) {
    return send_ws_message(wsheader_type::opcode_type::BINARY_FRAME, data, len, callback);
}

void WSClientImpl::reconnect() {}

bool WSClientImpl::on_transport_connected() {
    connected_ = true;
    constexpr size_t kBuffSize = 4096;
    std::shared_ptr<char> buff = std::shared_ptr<char>(new char[kBuffSize]);
    memset(buff.get(), 0, kBuffSize);
    std::vector<char> buff(kBuffSize, 0);
    const char* format = ""
                         "GET /%s HTTP/1.1\r\n"
                         "Host: %s:%d\r\n"
                         "Upgrade: websocket\r\n"
                         "Connection: Upgrade\r\n"
                         "Sec-WebSocket-Key: %s\r\n"
                         "Sec-WebSocket-Version: 13\r\n"
                         "\r\n";
    snprintf(buff.get(), kBuffSize, format, path_.c_str(), host_.c_str(), port_,
             rand16_base64_.c_str());
    Buffer buffer[1] = {{buff.get(), strlen(buff.get())}};
    return transport_->send(buffer, 1, [buff]() {
        // 确保buff的生命周期.
        LOG(VERBOSE) << "WebSocket HTTP upgrade request sent";
    });
}

void WSClientImpl::on_transport_closed() {
    connected_ = false;
    on_closed_();
}

void WSClientImpl::on_transport_reconnecting() {
    // ??
    connected_ = false;
    // parser_.clear();
    on_reconnecting_();
}

bool WSClientImpl::on_transport_read(const Buffer& buff) {
    switch (state_) {
    case WSState::Closed:
        LOG(WARNING) << "Received data in WSState::Closed";
        return false;
    case WSState::TcpConnecting:
        LOG(WARNING) << "Received data in WSState::TcpConnecting";
        return true;
    case WSState::SentHttp:
        buffer_.insert(buffer_.end(), reinterpret_cast<const uint8_t*>(buff.base),
                       reinterpret_cast<const uint8_t*>(buff.base) + buff.len);
        if (parse_http_response()) {
            return parse_ws();
        }
        else {
            return false;
        }
    case WSState::WSConnected:
        return parse_ws();
    default:
        LOG(ERR) << "Unknown WSState " << (int)state_;
        return false;
    }
}

bool WSClientImpl::parse_http_response() {
    std::vector<phr_header> headers(100, phr_header{});
    size_t num_headers = 0;
    int minor_version = 0;
    int status = 0;
    char* msg = nullptr;
    size_t msg_len = 0;
    const char* buff_begin = reinterpret_cast<const char*>(buffer_.data());
    int ret = phr_parse_response(buff_begin, buffer_.size(), &minor_version, &status, &msg,
                                 &msg_len, headers.data(), &num_headers, 0);
    if (ret > 0) {
        LOGF(INFO,
             "Parse websocket http upgrade response success, minor_version:%d, status:%d, "
             "num_headers:%u, content:\n%s",
             minor_version, status, num_headers,
             std::string(buff_begin, buff_begin + std::min(size_t(4096), buffer_.size())));
        if (status != 101) {
            LOG(ERR) << "Got bad status from websocket upgrade response";
            return false;
        }
        int header_length = msg - buff_begin;
        if (header_length <= 0) {
            LOG(ERR) << "Websocket http upgrade response header length invalid " << header_length;
            return false;
        }
        buffer_.erase(buffer_.begin(), buffer_.begin() + header_length);
        state_ = WSState::WSConnected;
        return true;
    }
    else if (ret == -1) {
        LOG(ERR) << "Parse websocket http upgrade response failed: "
                 << std::string(buff_begin, buff_begin + std::min(size_t(4096), buffer_.size()));
        return false;
    }
    else {
        // not enough content
        return true;
    }
}

bool WSClientImpl::parse_ws() {
    if (state_ != WSState::WSConnected) {
        return false;
    }
    while (true) {
        wsheader_type ws;
        if (buffer_.size() < 2) {
            return true; /* Need at least 2 */
        }
        const uint8_t* data = buffer_.data(); // peek, but don't consume
        ws.fin = (data[0] & 0x80) == 0x80;
        ws.opcode = (wsheader_type::opcode_type)(data[0] & 0x0f);
        ws.mask = (data[1] & 0x80) == 0x80;
        ws.N0 = (data[1] & 0x7f);
        ws.header_size = 2 + (ws.N0 == 126 ? 2 : 0) + (ws.N0 == 127 ? 8 : 0) + (ws.mask ? 4 : 0);
        if (buffer_.size() < ws.header_size) {
            return true; /* Need: ws.header_size - rxbuf.size() */
        }
        int i = 0;
        if (ws.N0 < 126) {
            ws.N = ws.N0;
            i = 2;
        }
        else if (ws.N0 == 126) {
            ws.N = 0;
            ws.N |= ((uint64_t)data[2]) << 8;
            ws.N |= ((uint64_t)data[3]) << 0;
            i = 4;
        }
        else if (ws.N0 == 127) {
            ws.N = 0;
            ws.N |= ((uint64_t)data[2]) << 56;
            ws.N |= ((uint64_t)data[3]) << 48;
            ws.N |= ((uint64_t)data[4]) << 40;
            ws.N |= ((uint64_t)data[5]) << 32;
            ws.N |= ((uint64_t)data[6]) << 24;
            ws.N |= ((uint64_t)data[7]) << 16;
            ws.N |= ((uint64_t)data[8]) << 8;
            ws.N |= ((uint64_t)data[9]) << 0;
            i = 10;
            if (ws.N & 0x8000000000000000ull) {
                // https://tools.ietf.org/html/rfc6455 writes the "the most
                // significant bit MUST be 0."
                //
                // We can't drop the frame, because (1) we don't we don't
                // know how much data to skip over to find the next header,
                // and (2) this would be an impractically long length, even
                // if it were valid. So just close() and return immediately
                // for now.
                LOG(ERR) << "Frame has invalid frame length.Closing.";
                return false;
            }
        }
        if (ws.mask) {
            ws.masking_key[0] = ((uint8_t)data[i + 0]) << 0;
            ws.masking_key[1] = ((uint8_t)data[i + 1]) << 0;
            ws.masking_key[2] = ((uint8_t)data[i + 2]) << 0;
            ws.masking_key[3] = ((uint8_t)data[i + 3]) << 0;
        }
        else {
            ws.masking_key[0] = 0;
            ws.masking_key[1] = 0;
            ws.masking_key[2] = 0;
            ws.masking_key[3] = 0;
        }

        // Note: The checks above should hopefully ensure this addition
        //       cannot overflow:
        if (buffer_.size() < ws.header_size + ws.N) {
            return true;
        }

        // We got a whole message, now do something with it:
        if (false) {
        }
        else if (ws.opcode == wsheader_type::TEXT_FRAME ||
                 ws.opcode == wsheader_type::BINARY_FRAME ||
                 ws.opcode == wsheader_type::CONTINUATION) {
            if (ws.mask) {
                for (size_t i = 0; i != ws.N; ++i) {
                    buffer_[i + ws.header_size] ^= ws.masking_key[i & 0x3];
                }
            }
            current_opcode_ =
                ws.opcode == wsheader_type::CONTINUATION ? current_opcode_ : ws.opcode;
            current_msg_.insert(current_msg_.end(), buffer_.begin() + ws.header_size,
                                buffer_.begin() + ws.header_size + (size_t)ws.N); // just feed
            if (ws.fin) {
                on_ws_message();
                current_msg_.clear();
                current_opcode_ = wsheader_type::CONTINUATION;
            }
        }
        else if (ws.opcode == wsheader_type::PING) {
            if (ws.mask) {
                for (size_t i = 0; i != ws.N; ++i) {
                    buffer_[i + ws.header_size] ^= ws.masking_key[i & 0x3];
                }
            }
            std::shared_ptr<uint8_t> payload = std::shared_ptr<uint8_t>(new uint8_t[ws.N]);
            memcpy(payload.get(), buffer_.data() + ws.header_size, (size_t)ws.N);
            send_ws_message(wsheader_type::PONG, payload, ws.N, nullptr);
        }
        else if (ws.opcode == wsheader_type::PONG) {
        }
        else if (ws.opcode == wsheader_type::CLOSE) {
            return false;
        }
        else {
            LOG(ERR) << "Got unexpected WebSocket message, opcode:" << (int)ws.opcode;
            return false;
        }

        buffer_.erase(buffer_.begin(), buffer_.begin() + ws.header_size + (size_t)ws.N);
    }
    return true;
}

bool WSClientImpl::on_ws_message() {
    bool ret = false;
    switch (current_opcode_) {
    case wsheader_type::BINARY_FRAME:
    {
        ret = true;
        break;
    }
    case wsheader_type::TEXT_FRAME:
    {
        ret = true;
        LOG(INFO) << "Received ws text message: "
                  << std::string(current_msg_.begin(), current_msg_.end());
        break;
    }
    default:
        LOG(DEBUG) << "Unknown ws message type: " << current_opcode_;
        break;
    }
    return ret;
}

bool WSClientImpl::send_ws_message(wsheader_type::opcode_type type,
                                   const std::shared_ptr<uint8_t>& data, uint32_t message_size,
                                   const std::function<void()>& callback) {
    uint8_t masking_key[4];
    *(uint32_t*)masking_key = randomIntegral<uint32_t>();
    if (state_ != WSState::WSConnected) {
        LOG(ERR) << "WebSocket is not connected, can't send ws message";
        return false;
    }
    Packet pkt{};
    pkt.header_length =
        2 + (message_size >= 126 ? 2 : 0) + (message_size >= 65536 ? 6 : 0) + (use_mask_ ? 4 : 0);
    uint8_t* header = pkt.header.get();
    header[0] = 0x80 | type;
    if (false) {
    }
    else if (message_size < 126) {
        header[1] = (message_size & 0xff) | (use_mask_ ? 0x80 : 0);
        if (use_mask_) {
            header[2] = masking_key[0];
            header[3] = masking_key[1];
            header[4] = masking_key[2];
            header[5] = masking_key[3];
        }
    }
    else if (message_size < 65536) {
        header[1] = 126 | (use_mask_ ? 0x80 : 0);
        header[2] = (message_size >> 8) & 0xff;
        header[3] = (message_size >> 0) & 0xff;
        if (use_mask_) {
            header[4] = masking_key[0];
            header[5] = masking_key[1];
            header[6] = masking_key[2];
            header[7] = masking_key[3];
        }
    }
    else { // TODO: run coverage testing here
        header[1] = 127 | (use_mask_ ? 0x80 : 0);
        header[2] = (message_size >> 56) & 0xff;
        header[3] = (message_size >> 48) & 0xff;
        header[4] = (message_size >> 40) & 0xff;
        header[5] = (message_size >> 32) & 0xff;
        header[6] = (message_size >> 24) & 0xff;
        header[7] = (message_size >> 16) & 0xff;
        header[8] = (message_size >> 8) & 0xff;
        header[9] = (message_size >> 0) & 0xff;
        if (use_mask_) {
            header[10] = masking_key[0];
            header[11] = masking_key[1];
            header[12] = masking_key[2];
            header[13] = masking_key[3];
        }
    }
    pkt.payload = std::shared_ptr<uint8_t>(new uint8_t[message_size]);
    memcpy(pkt.payload.get(), data.get(), message_size);
    pkt.payload_length = message_size;
    if (use_mask_) {
        for (size_t i = 0; i != message_size; ++i) {
            pkt.payload.get()[i] ^= masking_key[i & 0x3];
        }
    }
    Buffer buff[2] = {{(char*)pkt.header.get(), pkt.header_length},
                      {(char*)pkt.payload.get(), pkt.payload_length}};
    return transport_->send(buff, 2, [pkt, callback]() {
        // 把packet capture进来，是为了延续内部shared_ptr的生命周期
        if (callback != nullptr) {
            callback();
        }
    });
}

std::unique_ptr<Client> Client::create(const Params& params) {
    auto impl = std::make_shared<ClientImpl>(params);
    if (!impl->init()) {
        return nullptr;
    }
    std::unique_ptr<Client> client{new Client};
    client->impl_ = std::move(impl);
    return client;
}

bool Client::send(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg,
                  const std::function<void()>& callback) {
    return impl_->send(type, msg, callback);
}

bool Client::send(const std::shared_ptr<uint8_t>& data, uint32_t len,
                  const std::function<void()>& callback) {
    return impl_->send(data, len, callback);
}

void Client::reconnect() {
    impl_->reconnect();
}

} // namespace ltlib