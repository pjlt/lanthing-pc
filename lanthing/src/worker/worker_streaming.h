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

#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <string>

#include <ltlib/io/client.h>
#include <ltlib/io/ioloop.h>
#include <ltlib/settings.h>
#include <ltlib/system.h>
#include <ltlib/threads.h>

#include <transport/transport.h>

#include <audio/capturer/audio_capturer.h>
#include <inputs/executor/input_executor.h>
#include <message_handler.h>
#include <video/cepipeline/video_capture_encode_pipeline.h>
#include <worker/display_setting.h>
#include <worker/session_change_observer.h>
#include <worker/worker.h>

namespace lt {

namespace worker {

class WorkerStreaming : public Worker {
public:
    struct Params {
        std::string name;
        uint32_t width;
        uint32_t height;
        uint32_t refresh_rate;
        uint32_t monitor_index;
        bool need_negotiate;
        std::vector<lt::VideoCodecType> video_codecs;
        lt::AudioCodecType audio_codec;
    };

public:
    static std::tuple<std::unique_ptr<WorkerStreaming>, int32_t>
    create(std::map<std::string, std::string> options);
    ~WorkerStreaming() override;
    int wait() override;

private:
    WorkerStreaming(const Params& params);
    int32_t init();
    bool initPipeClient();
    bool saveAndChangeCurrentDisplaySettings(DisplaySettingNegotiator::Result result);
    void recoverDisplaySettings();
    bool negotiateAllParameters();
    int32_t negotiateStreamParameters();
    void getUserMaxMbps();
    void mainLoop(const std::function<void()>& i_am_alive);
    void stop(int exit_code);
    void postTask(const std::function<void()>& task);
    void postDelayTask(int64_t delay_ms, const std::function<void()>& task);
    bool registerMessageHandler(uint32_t type, const MessageHandler& msg);
    void dispatchServiceMessage(uint32_t type,
                                const std::shared_ptr<google::protobuf::MessageLite>& msg);
    bool sendPipeMessage(uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg);
    bool sendPipeMessageFromOtherThread(uint32_t type,
                                        const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void printStats();
    void checkCimeout();
    void updateInput();
    // TODO: AUDIO和VIDEO INPUT一样改成通用的接口
    void onCapturedAudioData(std::shared_ptr<google::protobuf::MessageLite> audio_data);

    // pipe message handlers
    void onPipeMessage(uint32_t type, std::shared_ptr<google::protobuf::MessageLite> msg);
    void onPipeDisconnected();
    void onPipeReconnecting();
    void onPipeConnected();
    void onStartWorking(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onStopWorking(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onKeepAlive(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onChangeStreamingParamsAck(const std::shared_ptr<google::protobuf::MessageLite>& msg);
    void onSwitchMonitor(const std::shared_ptr<google::protobuf::MessageLite>& msg);

private:
    const bool need_negotiate_;
    const uint32_t client_width_;
    const uint32_t client_height_;
    const uint32_t client_refresh_rate_;
    uint32_t monitor_index_;
    const std::vector<lt::VideoCodecType> client_codec_types_;
    const std::string pipe_name_;
    const AudioCodecType audio_codec_type_;
    bool connected_to_service_ = false;
    std::shared_mutex mutex_;
    std::unique_ptr<SessionChangeObserver> session_observer_;
    std::map<uint32_t, MessageHandler> msg_handlers_;
    // DisplaySetting negotiated_display_setting_;
    lt::VideoCodecType negotiated_video_codec_type_ = lt::VideoCodecType::Unknown;
    std::shared_ptr<google::protobuf::MessageLite> negotiated_params_;
    std::unique_ptr<ltlib::IOLoop> ioloop_;
    std::unique_ptr<ltlib::Client> pipe_client_;
    std::unique_ptr<ltlib::BlockingThread> thread_;
    int64_t last_time_received_from_service_;
    std::unique_ptr<lt::video::CaptureEncodePipeline> video_;
    std::unique_ptr<lt::input::Executor> input_;
    std::unique_ptr<lt::audio::Capturer> audio_;
    std::unique_ptr<ltlib::Settings> settings_;
    std::vector<ltlib::Monitor> monitors_;
    uint32_t max_mbps_ = 0;
    bool stoped_ = false;
};
} // namespace worker

} // namespace lt