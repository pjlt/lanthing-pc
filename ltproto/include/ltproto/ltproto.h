//这个文件是自动生成的，请不要手动修改.
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>
#include <deque>
#include <google/protobuf/message_lite.h>

namespace ltproto
{
    namespace peer2peer
    {
        class KeepAlive;
        class VideoFrame;
        class VideoFrameAck1;
        class VideoFrameAck2;
        class StartTransmission;
        class StartTransmissionAck;
        class StopTransmission;
        class StreamingParams;
        class StartWorking;
        class StartWorkingAck;
        class StopWorking;
        class KeyboardEvent;
        class MouseClick;
        class MouseMotion;
        class MouseWheel;
        class ControllerAddedRemoved;
        class ControllerStatus;
        class ControllerResponse;
    }
    namespace server
    {
        class LoginDevice;
        class LoginDeviceAck;
        class LoginUser;
        class LoginUserAck;
        class AllocateDeviceID;
        class AllocateDeviceIDAck;
        class RequestConnection;
        class RequestConnectionAck;
        class OpenConnection;
        class OpenConnectionAck;
        class CloseConnection;
    }
    namespace signaling
    {
        class SignalingMessage;
        class SignalingMessageAck;
        class JoinRoom;
        class JoinRoomAck;
    }
    namespace ui
    {
        class PushDeviceID;
    }

    constexpr uint32_t kMagicV1 = 0x414095;

#pragma pack(push, 1)
    struct PacketHeader
    {
        uint32_t magic : 24;
        uint32_t xor_key : 8;
        uint32_t payload_size;
        uint32_t checksum;
    };
#pragma pack(pop)

    constexpr uint32_t kMsgHeaderSize = sizeof(PacketHeader);
    static_assert(kMsgHeaderSize == 12);


    struct Message
    {
        uint32_t type;
        std::shared_ptr<google::protobuf::MessageLite> msg;
    };

    struct Packet
    {
        PacketHeader header;
        std::shared_ptr<uint8_t> payload;
        //头部暂时都是死的
        static std::optional<Packet> create(const Message& payload, bool need_xor);
    };
    namespace type
    {

        constexpr uint32_t kFirstProtocol = 0;
        constexpr uint32_t kKeepAlive = 1;
        constexpr uint32_t kVideoFrame = 14;
        constexpr uint32_t kVideoFrameAck1 = 15;
        constexpr uint32_t kVideoFrameAck2 = 16;
        constexpr uint32_t kStartTransmission = 17;
        constexpr uint32_t kStartTransmissionAck = 18;
        constexpr uint32_t kStopTransmission = 19;
        constexpr uint32_t kStreamingParams = 20;
        constexpr uint32_t kStartWorking = 21;
        constexpr uint32_t kStartWorkingAck = 22;
        constexpr uint32_t kStopWorking = 23;
        constexpr uint32_t kKeyboardEvent = 101;
        constexpr uint32_t kMouseClick = 102;
        constexpr uint32_t kMouseMotion = 103;
        constexpr uint32_t kMouseWheel = 104;
        constexpr uint32_t kControllerAddedRemoved = 105;
        constexpr uint32_t kControllerStatus = 106;
        constexpr uint32_t kControllerResponse = 107;
        constexpr uint32_t kLoginDevice = 1001;
        constexpr uint32_t kLoginDeviceAck = 1002;
        constexpr uint32_t kLoginUser = 1003;
        constexpr uint32_t kLoginUserAck = 1004;
        constexpr uint32_t kAllocateDeviceID = 1005;
        constexpr uint32_t kAllocateDeviceIDAck = 1006;
        constexpr uint32_t kRequestConnection = 3001;
        constexpr uint32_t kRequestConnectionAck = 3002;
        constexpr uint32_t kOpenConnection = 3003;
        constexpr uint32_t kOpenConnectionAck = 3004;
        constexpr uint32_t kCloseConnection = 3005;
        constexpr uint32_t kSignalingMessage = 2001;
        constexpr uint32_t kSignalingMessageAck = 2002;
        constexpr uint32_t kJoinRoom = 2003;
        constexpr uint32_t kJoinRoomAck = 2004;
        constexpr uint32_t kPushDeviceID = 4001;
        constexpr uint32_t kLastProtocol = 0xffffffff;
    } // namespace type


    std::shared_ptr<google::protobuf::MessageLite> create_by_type(uint32_t type);
    uint32_t id(const std::shared_ptr<peer2peer::KeepAlive>&);
    uint32_t id(const std::shared_ptr<peer2peer::VideoFrame>&);
    uint32_t id(const std::shared_ptr<peer2peer::VideoFrameAck1>&);
    uint32_t id(const std::shared_ptr<peer2peer::VideoFrameAck2>&);
    uint32_t id(const std::shared_ptr<peer2peer::StartTransmission>&);
    uint32_t id(const std::shared_ptr<peer2peer::StartTransmissionAck>&);
    uint32_t id(const std::shared_ptr<peer2peer::StopTransmission>&);
    uint32_t id(const std::shared_ptr<peer2peer::StreamingParams>&);
    uint32_t id(const std::shared_ptr<peer2peer::StartWorking>&);
    uint32_t id(const std::shared_ptr<peer2peer::StartWorkingAck>&);
    uint32_t id(const std::shared_ptr<peer2peer::StopWorking>&);
    uint32_t id(const std::shared_ptr<peer2peer::KeyboardEvent>&);
    uint32_t id(const std::shared_ptr<peer2peer::MouseClick>&);
    uint32_t id(const std::shared_ptr<peer2peer::MouseMotion>&);
    uint32_t id(const std::shared_ptr<peer2peer::MouseWheel>&);
    uint32_t id(const std::shared_ptr<peer2peer::ControllerAddedRemoved>&);
    uint32_t id(const std::shared_ptr<peer2peer::ControllerStatus>&);
    uint32_t id(const std::shared_ptr<peer2peer::ControllerResponse>&);
    uint32_t id(const std::shared_ptr<server::LoginDevice>&);
    uint32_t id(const std::shared_ptr<server::LoginDeviceAck>&);
    uint32_t id(const std::shared_ptr<server::LoginUser>&);
    uint32_t id(const std::shared_ptr<server::LoginUserAck>&);
    uint32_t id(const std::shared_ptr<server::AllocateDeviceID>&);
    uint32_t id(const std::shared_ptr<server::AllocateDeviceIDAck>&);
    uint32_t id(const std::shared_ptr<server::RequestConnection>&);
    uint32_t id(const std::shared_ptr<server::RequestConnectionAck>&);
    uint32_t id(const std::shared_ptr<server::OpenConnection>&);
    uint32_t id(const std::shared_ptr<server::OpenConnectionAck>&);
    uint32_t id(const std::shared_ptr<server::CloseConnection>&);
    uint32_t id(const std::shared_ptr<signaling::SignalingMessage>&);
    uint32_t id(const std::shared_ptr<signaling::SignalingMessageAck>&);
    uint32_t id(const std::shared_ptr<signaling::JoinRoom>&);
    uint32_t id(const std::shared_ptr<signaling::JoinRoomAck>&);
    uint32_t id(const std::shared_ptr<ui::PushDeviceID>&);


    class Parser
    {
    public:
        Parser() = default;
        void clear();
        void push_buffer(const uint8_t* buff, uint32_t size);
        bool parse_buffer();
        std::optional<Message> pop_message();

    private:
        bool parse_net_packets();
        int parse_net_packet(const uint8_t* data, uint32_t size, ltproto::Packet& packet);
        void parse_bussiness_messages();

    private:
        std::vector<uint8_t> buffer_;
        std::deque<Packet> packets_;
        std::deque<Message> messages_;
    };

} // namespace ltproto
