#include "client.h"
#include <g3log/g3log.hpp>

#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/keep_alive.pb.h>
#include <ltproto/peer2peer/start_transmission.pb.h>
#include <ltproto/peer2peer/start_transmission_ack.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>

#include <string_keys.h>

namespace {

rtc::VideoCodecType to_ltrtc(std::string codec_str) {
    static const std::string kAVC = "avc";
    static const std::string kHEVC = "hevc";
    std::transform(codec_str.begin(), codec_str.end(), codec_str.begin(), std::tolower);
    if (codec_str == kAVC) {
        return rtc::VideoCodecType::H264;
    }
    else if (codec_str == kHEVC) {
        return rtc::VideoCodecType::H265;
    }
    else {
        return rtc::VideoCodecType::Unknown;
    }
}

} // namespace

namespace lt {

namespace cli {

std::unique_ptr<Client> Client::create(std::map<std::string, std::string> options) {
    if (options.find("-cid") == options.end() || options.find("-rid") == options.end() ||
        options.find("-token") == options.end() || options.find("-user") == options.end() ||
        options.find("-pwd") == options.end() || options.find("-addr") == options.end() ||
        options.find("-port") == options.end() || options.find("-codec") == options.end() ||
        options.find("-width") == options.end() || options.find("-height") == options.end() ||
        options.find("-freq") == options.end() || options.find("-dinput") == options.end() ||
        options.find("-gamepad") == options.end()) {
        LOG(WARNING) << "Parameter invalid";
        return nullptr;
    }
    Params params{};
    params.client_id = options["-cid"];
    params.room_id = options["-rid"];
    params.auth_token = options["-token"];
    params.signaling_addr = options["-addr"];
    params.user = options["-user"];
    params.pwd = options["-pwd"];
    int32_t signaling_port = std::atoi(options["-port"].c_str());
    params.codec = options["-codec"];
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    params.enable_driver_input = std::atoi(options["-dinput"].c_str()) != 0;
    params.enable_gamepad = std::atoi(options["-gamepad"].c_str()) != 0;
    if (signaling_port <= 0 || signaling_port > 65535) {
        LOG(WARNING) << "Invalid parameter: port";
        return nullptr;
    }
    params.signaling_port = static_cast<uint16_t>(signaling_port);

    if (width <= 0) {
        LOG(WARNING) << "Invalid parameter: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);

    if (height <= 0) {
        LOG(WARNING) << "Invalid parameter: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);

    if (freq <= 0) {
        LOG(WARNING) << "Invalid parameter: freq";
        return nullptr;
    }
    params.screen_refresh_rate = static_cast<uint32_t>(freq);

    std::unique_ptr<Client> client{new Client{params}};
    if (!client->init()) {
        return false;
    }
    return client;
}

Client::Client(const Params& params)
    : auth_token_{params.auth_token}
    , p2p_username_{params.user}
    , p2p_password_{params.pwd}
    , signaling_params_{params.client_id, params.room_id, params.signaling_addr,
                        params.signaling_port}
    , video_params_{to_ltrtc(params.codec), params.width, params.height, params.screen_refresh_rate,
                    std::bind(&Client::send_message_to_host, this, std::placeholders::_1,
                              std::placeholders::_2, std::placeholders::_3)} {}

Client::~Client() {
    assert(ioloop_->is_not_current_thread());
    ioloop_->stop();
}

bool Client::init() {
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(WARNING) << "Init IOLoop failed";
        return false;
    }
    ltlib::Client::Params params{};
    params.stype = ltlib::StreamType::TCP;
    params.ioloop = ioloop_.get();
    params.host = signaling_params_.addr;
    params.port = signaling_params_.port;
    params.is_tls = false;
    params.on_connected = std::bind(&Client::on_signaling_connected, this);
    params.on_closed = std::bind(&Client::on_signaling_disconnected, this);
    params.on_reconnecting = std::bind(&Client::on_signaling_reconnecting, this);
    params.on_message = std::bind(&Client::on_signaling_net_message, this, std::placeholders::_1,
                                  std::placeholders::_2);
    signaling_client_ = ltlib::Client::create(params);
    if (signaling_client_ == nullptr) {
        LOG(INFO) << "Create signaling client failed";
        return false;
    }
    hb_thread_ = ltlib::TaskThread::create("heart_beat");
    main_thread_ = ltlib::BlockingThread::create(
        "main_thread",
        [this](const std::function<void()>& i_am_alive, void*) { main_loop(i_am_alive); }, nullptr);
    should_exit_ = false;
    return true;
}

void Client::wait() {
    std::unique_lock<std::mutex> lock{exit_mutex_};
    exit_cv_.wait(lock, [this]() { return should_exit_; });
}

void Client::main_loop(const std::function<void()>& i_am_alive) {
    LOG(INFO) << "Lanthing client enter main loop";
    ioloop_->run(i_am_alive);
}

void Client::on_platform_render_target_reset() {
    video_module_->reset_decoder_renderer();
}

void Client::on_platform_exit() {
    auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    msg->set_level(ltproto::signaling::SignalingMessage_Level_Core);
    auto coremsg = msg->mutable_core_message();
    coremsg->set_key(kSigCoreClose);
    ioloop_->post([this, msg]() {
        signaling_client_->send(ltproto::id(msg), msg, [this]() { stop_wait(); });
    });
}

void Client::stop_wait() {
    {
        std::lock_guard<std::mutex> lock{exit_mutex_};
        should_exit_ = true;
    }
    exit_cv_.notify_one();
}

void Client::on_signaling_net_message(uint32_t type,
                                      std::shared_ptr<google::protobuf::MessageLite> msg) {
    namespace ltype = ltproto::type;
    switch (type) {
    case ltype::kJoinRoomAck:
        on_join_room_ack(msg);
        break;
    case ltype::kSignalingMessage:
        on_signaling_message(msg);
        break;
    case ltype::kSignalingMessageAck:
        on_signaling_message_ack(msg);
        break;
    default:
        LOG(WARNING) << "Unknown signaling type";
        break;
    }
}

void Client::on_signaling_disconnected() {
    // TODO: 业务代码，目前是直接退出进程.
    stop_wait();
}

void Client::on_signaling_reconnecting() {
    LOG(INFO) << "Reconnecting signaling server...";
}

void Client::on_signaling_connected() {
    LOG(INFO) << "Connected to signaling server";
    auto msg = std::make_shared<ltproto::signaling::JoinRoom>();
    msg->set_session_id(signaling_params_.client_id);
    msg->set_room_id(signaling_params_.room_id);
    ioloop_->post([this, msg]() { signaling_client_->send(ltproto::id(msg), msg); });
}

void Client::on_join_room_ack(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::JoinRoomAck>(_msg);
    if (msg->err_code() != ltproto::signaling::JoinRoomAck_ErrCode_Success) {
        LOG(INFO) << "Join room " << signaling_params_.room_id << " with id "
                  << signaling_params_.client_id << " failed";
        return;
    }
    LOG(INFO) << "Join signaling room success";
    PcSdl::Params params;
    params.video_height = video_params_.height;
    params.video_width = video_params_.width;
    params.window_height = 720; // 默认值，后续通过屏幕大小计算.
    params.window_width = 1280; // 默认值，后续通过屏幕大小计算.
    params.on_reset = std::bind(&Client::on_platform_render_target_reset, this);
    params.on_exit = std::bind(&Client::on_platform_exit, this);
    sdl_ = PcSdl::create(params);
    if (sdl_ == nullptr) {
        LOG(INFO) << "Initialize sdl failed";
        return;
    }
    LOG(INFO) << "Initialize SDL success";
    video_params_.sdl = sdl_.get();
    input_params_.sdl = sdl_.get();
    if (!init_ltrtc()) {
        LOG(INFO) << "Initialize rtc failed";
        return;
    }
    LOG(INFO) << "Initialize rtc success";
}

void Client::on_signaling_message(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessage>(_msg);
    switch (msg->level()) {
    case ltproto::signaling::SignalingMessage::Core:
        // 暂时没有需要在这层处理的信令消息
        break;
    case ltproto::signaling::SignalingMessage::Rtc:
    {
        auto& rtc_msg = msg->rtc_message();
        rtc_client_->onSignalingMessage(rtc_msg.key().c_str(), rtc_msg.value().c_str());
        break;
    }
    default:
        break;
    }
}

void Client::on_signaling_message_ack(std::shared_ptr<google::protobuf::MessageLite> _msg) {
    auto msg = std::static_pointer_cast<ltproto::signaling::SignalingMessageAck>(_msg);
    switch (msg->err_code()) {
    case ltproto::signaling::SignalingMessageAck_ErrCode_Success:
        // do nothing
        break;
    case ltproto::signaling::SignalingMessageAck_ErrCode_NotOnline:
        LOG(INFO) << "Send signaling message failed, remote device not online";
        break;
    default:
        LOG(INFO) << "Send signaling message failed";
        break;
    }
}

bool Client::init_ltrtc() {
    namespace ph = std::placeholders;
    rtc::Client::Params cfg;
    cfg.use_nbp2p = false;
    cfg.username = p2p_username_.c_str();
    cfg.password = p2p_password_.c_str();
    cfg.on_data = std::bind(&Client::on_ltrtc_data, this, ph::_1, ph::_2, ph::_3);
    cfg.on_video = std::bind(&Client::on_ltrtc_video_frame, this, ph::_1);
    cfg.on_audio =
        std::bind(&Client::on_ltrtc_audio_data, this, ph::_1, ph::_2, ph::_3, ph::_4, ph::_5);
    cfg.on_connected = std::bind(&Client::on_ltrtc_connected, this);
    cfg.on_conn_changed = std::bind(&Client::on_ltrtc_conn_changed, this);
    cfg.on_failed = std::bind(&Client::on_ltrtc_failed, this);
    cfg.on_disconnected = std::bind(&Client::on_ltrtc_disconnected, this);
    cfg.on_signaling_message = std::bind(&Client::on_ltrtc_signaling_message, this, ph::_1, ph::_2);
    cfg.video_codec_type = video_params_.codec_type;
    rtc_client_ = rtc::Client::create(std::move(cfg));
    if (rtc_client_ == nullptr) {
        LOG(INFO) << "Create rtc client failed";
        return false;
    }
    if (!rtc_client_->connect()) {
        LOG(INFO) << "LTClient connect failed";
        return false;
    }
    return true;
}

void Client::on_ltrtc_data(const uint8_t* data, uint32_t size, bool is_reliable) {
    auto type = reinterpret_cast<const uint32_t*>(data);
    auto msg = ltproto::create_by_type(*type);
    if (msg == nullptr) {
        LOG(INFO) << "Unknown message type: " << *type;
    }
    bool success = msg->ParseFromArray(data + 4, size - 4);
    if (!success) {
        LOG(INFO) << "Parse message failed, type: " << *type;
        return;
    }
    dispatch_remote_message(*type, msg);
}

void Client::on_ltrtc_video_frame(const rtc::VideoFrame& frame) {
    Video::Action action = video_module_->submit(frame);
}

void Client::on_ltrtc_audio_data(uint32_t bits_per_sample, uint32_t sample_rate,
                                 uint32_t number_of_channels, const void* audio_data,
                                 uint32_t size) {
    // audio_module_->submit(bits_per_sample, sample_rate, number_of_channels,
    // reinterpret_cast<const uint8_t*>(audio_data), size);
}

void Client::on_ltrtc_connected() {
    video_module_ = Video::create(video_params_);
    if (video_module_ == nullptr) {
        LOG(WARNING) << "Create video module failed";
        return;
    }
    input_params_.send_message =
        std::bind(&Client::send_message_to_host, this, std::placeholders::_1, std::placeholders::_2,
                  std::placeholders::_3);
    input_params_.host_height = video_params_.height;
    input_params_.host_width = video_params_.width;
    input_module_ = Input::create(input_params_);
    if (input_module_ == nullptr) {
        LOG(WARNING) << "Create input module failed";
        return;
    }
    //  audio_ = client::Audio::create(sdl_.get());
    //  if (audio_ == nullptr) {
    //      LOG(INFO) << "Create audio module failed";
    //      return;
    //  }
    hb_thread_->post(std::bind(&Client::send_keep_alive, this));
    // 如果未来有“串流”以外的业务，在这个StartTransmission添加字段.
    auto start = std::make_shared<ltproto::peer2peer::StartTransmission>();
    start->set_client_os(ltproto::peer2peer::StartTransmission_ClientOS_Windows);
    start->set_token(auth_token_);
    send_message_to_host(ltproto::id(start), start, true);
}

void Client::on_ltrtc_conn_changed() {}

void Client::on_ltrtc_failed() {
    stop_wait();
}

void Client::on_ltrtc_disconnected() {
    stop_wait();
}

void Client::on_ltrtc_signaling_message(const std::string& key, const std::string& value) {
    // 将key和value封装在proto里.
    auto msg = std::make_shared<ltproto::signaling::SignalingMessage>();
    msg->set_level(ltproto::signaling::SignalingMessage::Rtc);
    auto rtc_msg = msg->mutable_rtc_message();
    rtc_msg->set_key(key);
    rtc_msg->set_value(value);
    ioloop_->post([this, msg]() { signaling_client_->send(ltproto::id(msg), msg); });
}

void Client::dispatch_remote_message(uint32_t type,
                                     const std::shared_ptr<google::protobuf::MessageLite>& msg) {
    switch (type) {
    case ltproto::type::kStartTransmissionAck:
        on_start_transmission_ack(msg);
        break;
    default:
        LOG(WARNING) << "Unknown message type: " << type;
        break;
    }
}

void Client::send_keep_alive() {
    auto keep_alive = std::make_shared<ltproto::peer2peer::KeepAlive>();
    send_message_to_host(ltproto::id(keep_alive), keep_alive, true);

    const auto k500ms = ltlib::TimeDelta{500'000};
    hb_thread_->post_delay(k500ms, std::bind(&Client::send_keep_alive, this));
}

bool Client::send_message_to_host(uint32_t type,
                                  const std::shared_ptr<google::protobuf::MessageLite>& msg,
                                  bool reliable) {
    auto packet = ltproto::Packet::create({type, msg}, false);
    if (!packet.has_value()) {
        LOG(WARNING) << "Create Peer2Peer packet failed, type:" << type;
        return false;
    }
    const auto& pkt = packet.value();
    // WebRTC的数据通道可以帮助我们完成stream->packet的过程，所以这里不需要把packet
    // header一起传过去.
    bool success = rtc_client_->sendData(pkt.payload.get(), pkt.header.payload_size, reliable);
    return success;
}

void Client::on_start_transmission_ack(const std::shared_ptr<google::protobuf::MessageLite>& _msg) {
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartTransmissionAck>(_msg);
    if (msg->err_code() == ltproto::peer2peer::StartTransmissionAck_ErrCode_Success) {
        LOG(INFO) << "Received StartTransmissionAck with success";
    }
    else {
        LOG(INFO) << "StartTransmission failed with "
                  << ltproto::peer2peer::StartTransmissionAck_ErrCode_Name(msg->err_code());
        stop_wait();
    }
}

} // namespace cli

} // namespace lt