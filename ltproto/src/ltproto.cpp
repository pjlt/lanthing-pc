//这个文件是自动生成的，请不要手动修改.
#include <ltproto/ltproto.h>
#include <ltproto/peer2peer/keep_alive.pb.h>
#include <ltproto/peer2peer/video_frame.pb.h>
#include <ltproto/peer2peer/video_frame_ack1.pb.h>
#include <ltproto/peer2peer/video_frame_ack2.pb.h>
#include <ltproto/peer2peer/start_transmission.pb.h>
#include <ltproto/peer2peer/start_transmission_ack.pb.h>
#include <ltproto/peer2peer/stop_transmission.pb.h>
#include <ltproto/peer2peer/streaming_params.pb.h>
#include <ltproto/peer2peer/start_working.pb.h>
#include <ltproto/peer2peer/start_working_ack.pb.h>
#include <ltproto/peer2peer/stop_working.pb.h>
#include <ltproto/peer2peer/keyboard_event.pb.h>
#include <ltproto/peer2peer/mouse_click.pb.h>
#include <ltproto/peer2peer/mouse_motion.pb.h>
#include <ltproto/peer2peer/mouse_wheel.pb.h>
#include <ltproto/peer2peer/controller_added_removed.pb.h>
#include <ltproto/peer2peer/controller_status.pb.h>
#include <ltproto/peer2peer/controller_response.pb.h>
#include <ltproto/server/login_device.pb.h>
#include <ltproto/server/login_device_ack.pb.h>
#include <ltproto/server/login_user.pb.h>
#include <ltproto/server/login_user_ack.pb.h>
#include <ltproto/server/allocate_device_id.pb.h>
#include <ltproto/server/allocate_device_id_ack.pb.h>
#include <ltproto/server/request_connection.pb.h>
#include <ltproto/server/request_connection_ack.pb.h>
#include <ltproto/server/open_connection.pb.h>
#include <ltproto/server/open_connection_ack.pb.h>
#include <ltproto/server/close_connection.pb.h>
#include <ltproto/signaling/signaling_message.pb.h>
#include <ltproto/signaling/signaling_message_ack.pb.h>
#include <ltproto/signaling/join_room.pb.h>
#include <ltproto/signaling/join_room_ack.pb.h>
#include <ltproto/ui/push_device_id.pb.h>


namespace ltproto
{

std::shared_ptr<google::protobuf::MessageLite> create_by_type(uint32_t _type)
{
    using namespace type;
    switch (_type) {
    case kKeepAlive:
        return std::make_shared<peer2peer::KeepAlive>();
    case kVideoFrame:
        return std::make_shared<peer2peer::VideoFrame>();
    case kVideoFrameAck1:
        return std::make_shared<peer2peer::VideoFrameAck1>();
    case kVideoFrameAck2:
        return std::make_shared<peer2peer::VideoFrameAck2>();
    case kStartTransmission:
        return std::make_shared<peer2peer::StartTransmission>();
    case kStartTransmissionAck:
        return std::make_shared<peer2peer::StartTransmissionAck>();
    case kStopTransmission:
        return std::make_shared<peer2peer::StopTransmission>();
    case kStreamingParams:
        return std::make_shared<peer2peer::StreamingParams>();
    case kStartWorking:
        return std::make_shared<peer2peer::StartWorking>();
    case kStartWorkingAck:
        return std::make_shared<peer2peer::StartWorkingAck>();
    case kStopWorking:
        return std::make_shared<peer2peer::StopWorking>();
    case kKeyboardEvent:
        return std::make_shared<peer2peer::KeyboardEvent>();
    case kMouseClick:
        return std::make_shared<peer2peer::MouseClick>();
    case kMouseMotion:
        return std::make_shared<peer2peer::MouseMotion>();
    case kMouseWheel:
        return std::make_shared<peer2peer::MouseWheel>();
    case kControllerAddedRemoved:
        return std::make_shared<peer2peer::ControllerAddedRemoved>();
    case kControllerStatus:
        return std::make_shared<peer2peer::ControllerStatus>();
    case kControllerResponse:
        return std::make_shared<peer2peer::ControllerResponse>();
    case kLoginDevice:
        return std::make_shared<server::LoginDevice>();
    case kLoginDeviceAck:
        return std::make_shared<server::LoginDeviceAck>();
    case kLoginUser:
        return std::make_shared<server::LoginUser>();
    case kLoginUserAck:
        return std::make_shared<server::LoginUserAck>();
    case kAllocateDeviceID:
        return std::make_shared<server::AllocateDeviceID>();
    case kAllocateDeviceIDAck:
        return std::make_shared<server::AllocateDeviceIDAck>();
    case kRequestConnection:
        return std::make_shared<server::RequestConnection>();
    case kRequestConnectionAck:
        return std::make_shared<server::RequestConnectionAck>();
    case kOpenConnection:
        return std::make_shared<server::OpenConnection>();
    case kOpenConnectionAck:
        return std::make_shared<server::OpenConnectionAck>();
    case kCloseConnection:
        return std::make_shared<server::CloseConnection>();
    case kSignalingMessage:
        return std::make_shared<signaling::SignalingMessage>();
    case kSignalingMessageAck:
        return std::make_shared<signaling::SignalingMessageAck>();
    case kJoinRoom:
        return std::make_shared<signaling::JoinRoom>();
    case kJoinRoomAck:
        return std::make_shared<signaling::JoinRoomAck>();
    case kPushDeviceID:
        return std::make_shared<ui::PushDeviceID>();
    default:
        return nullptr;
    }
}

uint32_t id(const std::shared_ptr<peer2peer::KeepAlive>&)
{
    return type::kKeepAlive;
}
uint32_t id(const std::shared_ptr<peer2peer::VideoFrame>&)
{
    return type::kVideoFrame;
}
uint32_t id(const std::shared_ptr<peer2peer::VideoFrameAck1>&)
{
    return type::kVideoFrameAck1;
}
uint32_t id(const std::shared_ptr<peer2peer::VideoFrameAck2>&)
{
    return type::kVideoFrameAck2;
}
uint32_t id(const std::shared_ptr<peer2peer::StartTransmission>&)
{
    return type::kStartTransmission;
}
uint32_t id(const std::shared_ptr<peer2peer::StartTransmissionAck>&)
{
    return type::kStartTransmissionAck;
}
uint32_t id(const std::shared_ptr<peer2peer::StopTransmission>&)
{
    return type::kStopTransmission;
}
uint32_t id(const std::shared_ptr<peer2peer::StreamingParams>&)
{
    return type::kStreamingParams;
}
uint32_t id(const std::shared_ptr<peer2peer::StartWorking>&)
{
    return type::kStartWorking;
}
uint32_t id(const std::shared_ptr<peer2peer::StartWorkingAck>&)
{
    return type::kStartWorkingAck;
}
uint32_t id(const std::shared_ptr<peer2peer::StopWorking>&)
{
    return type::kStopWorking;
}
uint32_t id(const std::shared_ptr<peer2peer::KeyboardEvent>&)
{
    return type::kKeyboardEvent;
}
uint32_t id(const std::shared_ptr<peer2peer::MouseClick>&)
{
    return type::kMouseClick;
}
uint32_t id(const std::shared_ptr<peer2peer::MouseMotion>&)
{
    return type::kMouseMotion;
}
uint32_t id(const std::shared_ptr<peer2peer::MouseWheel>&)
{
    return type::kMouseWheel;
}
uint32_t id(const std::shared_ptr<peer2peer::ControllerAddedRemoved>&)
{
    return type::kControllerAddedRemoved;
}
uint32_t id(const std::shared_ptr<peer2peer::ControllerStatus>&)
{
    return type::kControllerStatus;
}
uint32_t id(const std::shared_ptr<peer2peer::ControllerResponse>&)
{
    return type::kControllerResponse;
}
uint32_t id(const std::shared_ptr<server::LoginDevice>&)
{
    return type::kLoginDevice;
}
uint32_t id(const std::shared_ptr<server::LoginDeviceAck>&)
{
    return type::kLoginDeviceAck;
}
uint32_t id(const std::shared_ptr<server::LoginUser>&)
{
    return type::kLoginUser;
}
uint32_t id(const std::shared_ptr<server::LoginUserAck>&)
{
    return type::kLoginUserAck;
}
uint32_t id(const std::shared_ptr<server::AllocateDeviceID>&)
{
    return type::kAllocateDeviceID;
}
uint32_t id(const std::shared_ptr<server::AllocateDeviceIDAck>&)
{
    return type::kAllocateDeviceIDAck;
}
uint32_t id(const std::shared_ptr<server::RequestConnection>&)
{
    return type::kRequestConnection;
}
uint32_t id(const std::shared_ptr<server::RequestConnectionAck>&)
{
    return type::kRequestConnectionAck;
}
uint32_t id(const std::shared_ptr<server::OpenConnection>&)
{
    return type::kOpenConnection;
}
uint32_t id(const std::shared_ptr<server::OpenConnectionAck>&)
{
    return type::kOpenConnectionAck;
}
uint32_t id(const std::shared_ptr<server::CloseConnection>&)
{
    return type::kCloseConnection;
}
uint32_t id(const std::shared_ptr<signaling::SignalingMessage>&)
{
    return type::kSignalingMessage;
}
uint32_t id(const std::shared_ptr<signaling::SignalingMessageAck>&)
{
    return type::kSignalingMessageAck;
}
uint32_t id(const std::shared_ptr<signaling::JoinRoom>&)
{
    return type::kJoinRoom;
}
uint32_t id(const std::shared_ptr<signaling::JoinRoomAck>&)
{
    return type::kJoinRoomAck;
}
uint32_t id(const std::shared_ptr<ui::PushDeviceID>&)
{
    return type::kPushDeviceID;
}

std::optional<Packet> Packet::create(const Message& payload, bool need_xor)
{
    need_xor = false; // XXX
    Packet pkt{};
    pkt.header.magic = kMagicV1;
    pkt.header.xor_key = 0;
    pkt.header.checksum = 0;
    pkt.header.payload_size = payload.msg->ByteSizeLong() + 4;
    pkt.payload = std::shared_ptr<uint8_t>(new uint8_t[pkt.header.payload_size]);
    *(uint32_t*)pkt.payload.get() = payload.type;
    if (payload.msg->SerializeToArray(pkt.payload.get() + 4, pkt.header.payload_size - 4)) {
        if (need_xor) {
            pkt.header.xor_key = ::rand() % 254 + 1;
            for (uint32_t i = 0; i < pkt.header.payload_size; i++) {
                *pkt.payload.get() ^= (uint8_t)pkt.header.xor_key;
            }
        }
        return pkt;
    }
    else {
        return std::nullopt;
    }
}

void Parser::clear()
{
    buffer_.clear();
    packets_.clear();
    messages_.clear();
}

void Parser::push_buffer(const uint8_t* buff, uint32_t size)
{
    buffer_.insert(buffer_.cend(), buff, buff + size);
}

bool Parser::parse_buffer()
{
    if (!parse_net_packets()) {
        return false;
    }
    parse_bussiness_messages();
    return true;
}

std::optional<Message> Parser::pop_message()
{
    if (messages_.empty()) {
        return std::nullopt;
    }
    else {
        auto top = messages_.front();
        messages_.pop_front();
        return top;
    }
}

bool Parser::parse_net_packets()
{
    size_t total_erase_size = 0;
    while (buffer_.size() - total_erase_size > 0) {
        ltproto::Packet packet;
        int ret_value = parse_net_packet(
            buffer_.data() + total_erase_size,
            buffer_.size() - total_erase_size,
            packet);
        if (ret_value > 0) {
            total_erase_size += ret_value;
            packets_.push_back(packet);
            continue;
        }
        else if (ret_value < 0) {
            break; // 数据不足
        }
        else if (ret_value == 0) {
            return false; // 解析异常
        }
    }

    if (buffer_.size() == total_erase_size)
        buffer_.clear();
    else
        buffer_.erase(buffer_.begin(), buffer_.begin() + total_erase_size);

    return true;
}
int Parser::parse_net_packet(const uint8_t* data, uint32_t size, ltproto::Packet& packet)
{
    if (size < ltproto::kMsgHeaderSize) {
        return -1;
    }
    packet.header = *reinterpret_cast<const ltproto::PacketHeader*>(data);
    // 长度是否足够一个包
    if (size < packet.header.payload_size + ltproto::kMsgHeaderSize) {
        return -1;
    }
    // 长度是否超出20MB限制
    if (size > 20 * 1024 * 1024) {
        return 0;
    }
    std::shared_ptr<uint8_t> payload{ new uint8_t[packet.header.payload_size] };
    ::memcpy(payload.get(), data + ltproto::kMsgHeaderSize, packet.header.payload_size);
    if (packet.header.xor_key != 0) {
        for (uint32_t i = 0; i < packet.header.payload_size; i++) {
            *(payload.get() + i) ^= packet.header.xor_key;
        }
    }
    packet.payload = payload;
    return static_cast<int>(packet.header.payload_size + ltproto::kMsgHeaderSize);
}
void Parser::parse_bussiness_messages()
{
    while (!packets_.empty()) {
        auto& packet = packets_.front();
        uint32_t type = *reinterpret_cast<const uint32_t*>(packet.payload.get());
        std::shared_ptr<google::protobuf::MessageLite> msg = create_by_type(type);
        if (msg == nullptr) {
            //unknown msg type
        }
        else {
            bool success = msg->ParseFromArray(packet.payload.get() + 4, packet.header.payload_size - 4);
            if (!success) {
                //error
            }
            else {
                messages_.push_back({ type, msg });
            }

        }
        packets_.pop_front();
    }
}

} // namespace ltproto
