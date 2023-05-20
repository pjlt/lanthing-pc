import json
import sys
import os

NOTE = '//这个文件是自动生成的，请不要手动修改.'

HEADER_INCLUDE = '''
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include <optional>
#include <deque>
#include <google/protobuf/message_lite.h>
'''

PACKET_STRUCTURE = '''
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
'''

CREATE_BY_TYPE = '    std::shared_ptr<google::protobuf::MessageLite> create_by_type(uint32_t type);'

PARSER_DECLAYRE = '''
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
'''

NAMESPACE_START = '''
namespace ltproto
{
'''
NAMESPACE_END = '''
} // namespace ltproto
'''

NAMESPACE_TYPE_START = '''    namespace type
    {
'''
NAMESPACE_TYPE_END = '''
    } // namespace type
'''

PARSER_IMPLEMENT = '''
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
'''

CPP_INCLUDE = '#include <ltproto/ltproto.h>\n'

CREATE_BY_TYPE_START = '''
std::shared_ptr<google::protobuf::MessageLite> create_by_type(uint32_t _type)
{
    using namespace type;
    switch (_type) {
'''
CREATE_BY_TYPE_END = '''    default:
        return nullptr;
    }
}
'''

id_array = []
id_set = set()
message_array = []
message_set = set()
sub_ns = []
proto_h_includes = []

def generate_forward_declare(ns: str) -> str:
    content = ' ' * 4 + f"namespace {ns}\n"
    content += ' ' * 4 + '{\n'
    for message in message_array:
        if message['ns'] == ns:
            content += ' ' * 8 + f"class {message['message']};\n"
    content += ' ' * 4 + "}\n"
    return content

def generate_id_func_declare() -> str:
    content = ''
    for message in message_array:
        content += f"{' ' * 4}uint32_t id(const std::shared_ptr<{message['ns']}::{message['message']}>&);\n"
    return content

def generate_id_func_implementation() -> str:
    content = ''
    for message in message_array:
        content += f"uint32_t id(const std::shared_ptr<{message['ns']}::{message['message']}>&)\n"
        content += "{\n"
        content += ' ' * 4 + f"return type::k{message['message']};\n"
        content += "}\n"
    return content


def generate_cpp_header() -> str:
    full_header_content:str = NOTE
    full_header_content += HEADER_INCLUDE
    full_header_content += NAMESPACE_START
    for ns in sub_ns:
        full_header_content += generate_forward_declare(ns)
    full_header_content += PACKET_STRUCTURE
    full_header_content += NAMESPACE_TYPE_START
    full_header_content += '\n'
    full_header_content += ' ' * 8 + 'constexpr uint32_t kFirstProtocol = 0;\n'
    for idx in range(0, len(id_array)):
        full_header_content += f'{" " * 8}constexpr uint32_t k{message_array[idx]["message"]} = {id_array[idx]};\n'
    full_header_content += ' ' * 8 + 'constexpr uint32_t kLastProtocol = 0xffffffff;'
    full_header_content += NAMESPACE_TYPE_END
    full_header_content += '\n\n'
    full_header_content += CREATE_BY_TYPE + '\n'
    full_header_content += generate_id_func_declare()
    full_header_content += '\n'
    full_header_content += PARSER_DECLAYRE
    full_header_content += NAMESPACE_END
    return full_header_content

def generate_cpp_cpp() -> str:
    full_cpp_content:str = NOTE + "\n"
    full_cpp_content += CPP_INCLUDE
    for include in proto_h_includes:
        full_cpp_content += include + '\n'
    full_cpp_content += '\n'
    full_cpp_content += NAMESPACE_START
    full_cpp_content += CREATE_BY_TYPE_START
    for message in message_array:
        full_cpp_content += f'{" " * 4}case k{message["message"]}:\n'
        full_cpp_content += f'{" " * 8}return std::make_shared<{message["ns"]}::{message["message"]}>();\n'
    full_cpp_content += CREATE_BY_TYPE_END + '\n'
    full_cpp_content += generate_id_func_implementation()
    full_cpp_content += PARSER_IMPLEMENT
    full_cpp_content += NAMESPACE_END
    return full_cpp_content

def parse_defines_json():
    json_file = open(os.path.dirname(__file__) + '/defines.json', encoding='utf-8')
    if json_file.closed:
        print("Can't open 'defines.json'", file=sys.stderr)
        exit(-1)

    json_obj = json.load(json_file)
    for ns in json_obj:
        sub_ns.append(ns)
        for entry in json_obj[ns]:
            id_array.append(entry['id'])
            if entry['id'] in id_set:
                print(f"Duplicate id '{entry['id']}'", file=sys.stderr)
                exit(-1)
            id_set.add(entry['id'])
            message_array.append({'ns':ns, 'message':entry['message']})
            if entry['message'] in message_set:
                print(f"Duplicate message '{entry['message']}'", file=sys.stderr)
                exit(-1)
            message_set.add(entry['message'])
            proto_h_includes.append(f'#include <ltproto/{ns}/{entry["proto"].replace("proto", "pb.h")}>')

def write_file(path: str, content: str):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(content)

def main():
    parse_defines_json()
    full_header_content = generate_cpp_header()
    write_file(os.path.dirname(__file__) + '/include/ltproto/ltproto.h', full_header_content)
    full_cpp_content = generate_cpp_cpp()
    write_file(os.path.dirname(__file__) + '/src/ltproto.cpp', full_cpp_content)

main()