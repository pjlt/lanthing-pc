#include "worker.h"
#include <g3log/g3log.hpp>
#include <ltproto/peer2peer/start_working.pb.h>
#include <ltproto/peer2peer/start_working_ack.pb.h>
#include <ltproto/peer2peer/streaming_params.pb.h>
#include <ltproto/ltproto.h>
#include <ltlib/times.h>

namespace
{

ltrtc::VideoCodecType to_ltrtc(ltproto::peer2peer::VideoCodecType codec_type)
{
    switch (codec_type) {
    case ltproto::peer2peer::AVC:
        return ltrtc::VideoCodecType::H264;
    case ltproto::peer2peer::HEVC:
        return ltrtc::VideoCodecType::H265;
    default:
        return ltrtc::VideoCodecType::Unknown;
    }
}

ltproto::peer2peer::StreamingParams::VideoEncodeBackend to_protobuf(lt::VideoEncoder::Backend backend)
{
    switch (backend) {
    case lt::VideoEncoder::Backend::NvEnc:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_NvEnc;
    case lt::VideoEncoder::Backend::IntelMediaSDK:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_IntelMediaSDK;
    case lt::VideoEncoder::Backend::Amf:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_AMF;
    default:
        return ltproto::peer2peer::StreamingParams_VideoEncodeBackend_Unknown;
    }
}

ltproto::peer2peer::VideoCodecType to_protobuf(ltrtc::VideoCodecType codec_type)
{
    switch (codec_type) {
    case ltrtc::VideoCodecType::H264:
        return ltproto::peer2peer::VideoCodecType::AVC;
    case ltrtc::VideoCodecType::H265:
        return ltproto::peer2peer::VideoCodecType::HEVC;
    default:
        return ltproto::peer2peer::VideoCodecType::Unknown;
    }
}

std::string to_string(ltrtc::VideoCodecType type)
{
    switch (type) {
    case ltrtc::VideoCodecType::H264:
        return "AVC";
    case ltrtc::VideoCodecType::H265:
        return "HEVC";
    default:
        return "Unknown Codec";
    }
}

} // 匿名空间

namespace lt
{

namespace worker
{

std::unique_ptr<Worker> Worker::create(std::map<std::string, std::string> options)
{
    if (options.find("-width") == options.end()
        || options.find("-height") == options.end()
        || options.find("-freq") == options.end()
        || options.find("-codecs") == options.end()
        || options.find("-name") == options.end()) {
        LOG(WARNING) << "Parameter invalid";
        return nullptr;
    }
    Params params {};
    int32_t width = std::atoi(options["-width"].c_str());
    int32_t height = std::atoi(options["-height"].c_str());
    int32_t freq = std::atoi(options["-freq"].c_str());
    std::stringstream ss(options["-codecs"]);

    params.name = options["-name"];
    if (params.name.empty()) {
        LOG(WARNING) << "Parameter invalid: name";
        return nullptr;
    }
    if (width <= 0) {
        LOG(WARNING) << "Parameter invalid: width";
        return nullptr;
    }
    params.width = static_cast<uint32_t>(width);
    if (height <= 0) {
        LOG(WARNING) << "Parameter invalid: height";
        return nullptr;
    }
    params.height = static_cast<uint32_t>(height);
    if (freq <= 0) {
        LOG(WARNING) << "Parameter invalid: freq";
        return nullptr;
    }
    params.refresh_rate = static_cast<uint32_t>(freq);
    std::string codec;
    while (std::getline(ss, codec)) {
        if (codec == "avc") {
            params.codecs.push_back(ltrtc::VideoCodecType::H264);
        } else if (codec == "hevc") {
            params.codecs.push_back(ltrtc::VideoCodecType::H265);
        }
    }
    if (params.codecs.empty()) {
        LOG(WARNING) << "Parameter invalid: codecs";
        return nullptr;
    }

    std::unique_ptr<Worker> worker { new Worker { params } };
    if (!worker->init()) {
        return nullptr;
    }
    return worker;
}

Worker::Worker(const Params& params)
    : client_width_ { params.width }
    , client_height_ { params.height }
    , client_refresh_rate_ { params.refresh_rate }
    , client_codec_types_ { params.codecs }
    , pipe_name_ { params.name }
    , last_time_received_from_service_ { ltlib::steady_now_ms() }
{
}

bool Worker::init()
{
    session_observer_ = SessionChangeObserver::create();
    if (session_observer_ == nullptr) {
        LOG(WARNING) << "Create session observer failed";
        return false;
    }
    ioloop_ = ltlib::IOLoop::create();
    if (ioloop_ == nullptr) {
        LOG(WARNING) << "Create IOLoop failed";
        return false;
    }
    if (!init_pipe_client()) {
        LOG(WARNING) << "Init pipe client failed";
        return false;
    }
    DisplaySetting client_display_setting { client_width_, client_height_, client_refresh_rate_ };
    negotiated_display_setting_ = DisplaySettingNegotiator::negotiate(client_display_setting);
    if (negotiated_display_setting_.width == 0 || negotiated_display_setting_.height == 0) {
        LOG(WARNING) << "Negotiate display setting failed, fallback to default(width:1920, height:1080, refresh_rate:60)";
        negotiated_display_setting_.width = 1920;
        negotiated_display_setting_.height = 1080;
    }
    negotiate_parameters();

    ioloop_->post_delay(500 /*ms*/, std::bind(&Worker::check_timeout, this));
    std::promise<void> promise;
    auto future = promise.get_future();
    thread_ = ltlib::BlockingThread::create(
        "main_thread", [this, &promise](const std::function<void()>& i_am_alive, void*) {
        promise.set_value();
        main_loop(i_am_alive);
        },
        nullptr);
    future.get();
    return true;
}

bool Worker::init_pipe_client()
{
    ltlib::Client::Params params {};
    params.stype = ltlib::StreamType::Pipe;
    params.ioloop = ioloop_.get();
    params.pipe_name = pipe_name_;
    params.is_tls = false;
    params.on_closed = std::bind(&Worker::on_pipe_disconnected, this);
    params.on_connected = std::bind(&Worker::on_pipe_connected, this);
    params.on_message = std::bind(&Worker::on_pipe_message, this, std::placeholders::_1, std::placeholders::_2);
    params.on_reconnecting = std::bind(&Worker::on_pipe_reconnecting, this);
    pipe_client_ = ltlib::Client::create(params);
    return pipe_client_ != nullptr;
}

void Worker::negotiate_parameters()
{
    // TODO: 检测抓屏能力
    std::vector<VideoEncoder::Ability> encode_abilities = VideoEncoder::check_encode_abilities(client_width_, client_height_);
    auto negotiated_params = std::make_shared<ltproto::peer2peer::StreamingParams>();
    negotiated_params->set_enable_driver_input(false);
    negotiated_params->set_enable_gamepad(false);
    negotiated_params->set_screen_refresh_rate(negotiated_display_setting_.refrash_rate);
    negotiated_params->set_video_width(negotiated_display_setting_.width);
    negotiated_params->set_video_height(negotiated_display_setting_.height);
    auto negotiated_video_codecs = negotiated_params->mutable_video_codecs();
    for (const auto& encode_ability : encode_abilities) {
        if (!negotiated_video_codecs->empty()) {
            break;
        }
        for (const auto& client_codec_type : client_codec_types_) {
            if (client_codec_type == encode_ability.codec_type) {
                ltproto::peer2peer::StreamingParams::VideoCodec video_codec;
                video_codec.set_backend(::to_protobuf(encode_ability.backend));
                video_codec.set_codec_type(::to_protobuf(encode_ability.codec_type));
                negotiated_video_codecs->Add(std::move(video_codec));
                negotiated_video_codec_beckend_ = encode_ability.backend;
                negotiated_video_codec_type_ = encode_ability.codec_type;
                LOG(INFO) << "Negotiated video codec:" << to_string(encode_ability.codec_type);
                break;
            }
        }
    }
    if (negotiated_video_codecs->empty()) {
        std::stringstream ss;
        ss << "Negotiate video codec failed, client supports codec:[";
        for (auto codec : client_codec_types_) {
            ss << to_string(codec) << " ";
        }
        ss << "], host supports codec:[";
        for (auto& codec : encode_abilities) {
            ss << to_string(codec.codec_type) << " ";
        }
        ss << "]";
        LOG(WARNING) << ss.str();
    }
    negotiated_params_ = negotiated_params;
}

void Worker::main_loop(const std::function<void()>& i_am_alive)
{
    LOG(INFO) << "Worker enter main loop";
    ioloop_->run(i_am_alive);
}

void Worker::stop()
{
    //NOTE: 只能由ioloop以外的线程调用
    ioloop_->stop();
}

bool Worker::register_message_handler(uint32_t type, const MessageHandler& handler)
{
    auto [_, success] = msg_handlers_.insert({ type, handler });
    if (!success) {
        LOG(WARNING) << "Register message handler(" << type << ") failed";
        return false;
    } else {
        return true;
    }
}

void Worker::dispatch_service_message(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg)
{
    auto iter = msg_handlers_.find(type);
    if (iter == msg_handlers_.cend() || iter->second == nullptr) {
        LOG(WARNING) << "Unknown message type: " << type;
        return;
    }
    iter->second(msg);
}

bool Worker::send_pipe_message(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg)
{
    if (!connected_to_service_) {
        return false;
    }
    return pipe_client_->send(type, msg);
}

void Worker::print_stats()
{
}

void Worker::check_timeout()
{
    constexpr int64_t kTimeout = 3'000;
    auto now = ltlib::steady_now_ms();
    if (now - last_time_received_from_service_ > kTimeout) {
        //stop(); // check_timeout()是跑在ioloop里的，而stop只能由ioloop以外的线程执行
        std::exit(0); //想一个更好的办法退出
    } else {
        ioloop_->post_delay(500, std::bind(&Worker::check_timeout, this));
    }
}

void Worker::on_pipe_message(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg)
{
    dispatch_service_message(type, msg);
}

void Worker::on_pipe_disconnected()
{
    LOG(WARNING) << "Disconnected from service, won't reconnect again";
    connected_to_service_ = false;
}

void Worker::on_pipe_reconnecting()
{
    LOG(INFO) << "Reconnecting to service...";
    connected_to_service_ = false;
}

void Worker::on_pipe_connected()
{
    if (connected_to_service_) {
        LOG(FATAL) << "Logic error, connected to service twice";
    } else {
        LOG(INFO) << "Connected to service";
    }
    connected_to_service_ = true;
    // 连上第一时间，向service发送协商好的串流参数
    auto params = std::static_pointer_cast<ltproto::peer2peer::StreamingParams>(negotiated_params_);
    send_pipe_message(ltproto::id(params), params);
}

void Worker::on_start_working(const std::shared_ptr<google::protobuf::MessageLite>& _msg)
{
    auto msg = std::static_pointer_cast<ltproto::peer2peer::StartWorking>(_msg);
    auto ack = std::make_shared<ltproto::peer2peer::StartWorkingAck>();
    do {
        host::VideoParams video_params {};
        video_params._metrics = metrics_.get();
        video_params.height = negotiated_display_setting_.height;
        video_params.width = negotiated_display_setting_.width;
        video_params.refresh_rate = negotiated_display_setting_.refrash_rate;
        video_params.codec_type = negotiated_video_codec_type_;
        video_params.encoder_backend = negotiated_video_codec_beckend_;
        video_params.send_message = std::bind(&Host::send_ipc_message, this, std::placeholders::_1, std::placeholders::_2);
        video_params.register_message_handler = std::bind(&Host::register_message_handler, this, std::placeholders::_1, std::placeholders::_2);
        video_ = host::Video::create(video_params);
        if (video_ == nullptr) {
            ack->set_err_code(ltproto::peer2peer::StartWorkingAck_ErrCode_VideoFailed);
            break;
        }
        // host::AudioParams audio_params {};
        // audio_params.send_message = std::bind(&Host::send_ipc_message, this, std::placeholders::_1, std::placeholders::_2);
        // audio_ = host::Audio::create(audio_params);
        // if (audio_ == nullptr) {
        //     video_->stop();
        //     ack->set_err_code(ltproto::peer2peer::StartWorkingAck_ErrCode_AudioFailed);
        //     break;
        // }
        // InputParams input_params {};
        // input_params.send_message = std::bind(&Host::send_ipc_message, this, std::placeholders::_1, std::placeholders::_2);
        // input_params.register_message_handler = std::bind(&Host::register_message_handler, this, std::placeholders::_1, std::placeholders::_2);
        // input_ = host::Input::create(input_params);
        // if (input_ == nullptr) {
        //     video_->stop();
        //     audio_->stop();
        //     ack->set_err_code(ltproto::peer2peer::StartWorkingAck_ErrCode_InputFailed);
        //     break;
        // }
        ack->set_err_code(ltproto::peer2peer::StartWorkingAck_ErrCode_Success);
    } while (false);
    for (const auto& handler : msg_handlers_) {
        ack->add_msg_type(handler.first);
    }
    send_pipe_message(ltproto::id(ack), ack);
}

void Worker::on_stop_working(const std::shared_ptr<google::protobuf::MessageLite>& msg)
{
    LOG(INFO) << "Received StopWorking";
    // stop(); // check_timeout()是跑在ioloop里的，而stop只能由ioloop以外的线程执行
    std::exit(0); // 想一个更好的办法退出
}

void Worker::on_keep_alive(const std::shared_ptr<google::protobuf::MessageLite>& msg)
{
    last_time_received_from_service_ = ltlib::steady_now_ms();
}

} // namespace worker

} // namespace lt