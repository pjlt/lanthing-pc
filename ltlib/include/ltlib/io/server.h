#pragma once
#include <ltlib/ltlib.h>
#include <ltlib/io/types.h>
#include <ltlib/io/ioloop.h>
#include <cstdint>
#include <string>
#include <google/protobuf/message_lite.h>

namespace ltlib
{

class ServerImpl;

//不支持TLS
class LT_API Server
{
public:
	struct Params
	{
		StreamType stype;
		IOLoop* ioloop;
		std::string pipe_name;
		std::string bind_ip;
		uint16_t bind_port;
		std::function<void(uint32_t)> on_accepted;
		std::function<void(uint32_t)> on_closed;
		std::function<void(uint32_t/*fd*/, uint32_t/*type*/, const std::shared_ptr<google::protobuf::MessageLite>&)> on_message;
	};

public:
	static std::unique_ptr<Server> create(const Params& params);
	bool send(uint32_t fd, uint32_t type, const std::shared_ptr<google::protobuf::MessageLite>& msg, const std::function<void()>& callback = nullptr);
	// 当上层调用send()返回false时，由上层调用close()关闭这个fd。此时on_closed将被回调
	void close(uint32_t fd);

private:
	std::shared_ptr<ServerImpl> impl_;
};

} // namespace ltlib