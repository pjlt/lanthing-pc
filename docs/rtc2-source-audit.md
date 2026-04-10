# rtc2 模块源码审计报告

- 模块路径: `src/transport/rtc2`
- 审计日期: 2026-04-11
- 审计方式: 静态代码走查（架构、流程、功能、风险、测试覆盖）

## 1. 总览

1. rtc2 当前为“可继续开发的传输框架”，但距离稳定可上线仍有明显差距。
2. 模块采用适配层 + 连接编排层 + 网络/P2P/DTLS/消息/媒体分层设计，结构方向正确。
3. 存在多处高风险问题：未初始化指针调用、参数透传断裂、证书校验放空。
4. P2P 仅 LAN 路径相对可用，WAN/Relay 实现尚未闭环。
5. 测试覆盖集中在 Buffer 与 RTP 基础能力，缺少连接/握手/弱网/并发场景测试。

## 2. 架构

### 2.1 分层结构

1. 适配层
- 文件: `src/transport/rtc2/include/rtc2/rtc2.h`, `src/transport/rtc2/src/rtc2.cpp`
- 职责: 对接 `transport_api`，桥接回调和数据结构。

2. 连接编排层
- 文件: `src/transport/rtc2/include/rtc2/connection.h`, `src/transport/rtc2/src/connection/connection_impl.cpp`
- 职责: 组装并协调 NetworkChannel、DtlsChannel、MessageChannel、音视频流。

3. 网络与连通层
- 文件: `src/transport/rtc2/src/modules/network/*`, `src/transport/rtc2/src/modules/p2p/*`
- 职责: UDP 收发、事件循环、候选地址交换、P2P 连通。

4. 安全与消息层
- 文件: `src/transport/rtc2/src/modules/dtls/*`, `src/transport/rtc2/src/stream/message_channel.cpp`, `src/transport/rtc2/src/modules/message/*`
- 职责: DTLS 握手和加解密，可靠/半可靠消息分发。

5. 媒体层
- 文件: `src/transport/rtc2/src/stream/video_*`, `src/transport/rtc2/src/stream/audio_*`, `src/transport/rtc2/src/modules/video/*`, `src/transport/rtc2/src/modules/rtp/*`
- 职责: RTP 打包/解包、视频组帧、音频/视频流收发。

### 2.2 依赖方向

- `Client/Server -> Connection -> ConnectionImpl -> (Network/P2P/DTLS/Message/Streams)`
- 顶层向下依赖，回调自底向上回传。

## 3. 端到端流程

1. 创建连接
- `Client::create/Server::create` 构造 `Connection::Params` 并调用 `Connection::create`。

2. 初始化核心组件
- `ConnectionImpl::init` 创建：
  - `NetworkChannel`
  - `Pacer`
  - `VideoSendStream/VideoReceiveStream`
  - `AudioSendStream/AudioReceiveStream`
  - `DtlsChannel`
  - `MessageChannel`

3. 启动网络
- `ConnectionImpl::start` -> `NetworkChannel::start` -> `P2P::maybe_start`。

4. 候选地址交换
- 本端通过 `onEndpointInfo` 生成 signaling 消息。
- 远端地址经 `onSignalingMessage` 注入 `NetworkChannel::addRemoteInfo`。

5. DTLS 握手
- 底层连通后 `DtlsChannel::onNetworkConnected` 触发 `startHandshake`。
- 握手成功后 `ConnectionImpl::onDtlsConnected` 通知上层 `on_connected`。

6. 数据面收发
- 消息: `MessageChannel::sendMessage/onRecvData`（当前强制走 reliable）。
- 媒体: RTP 包经 DTLS/网络收发，视频在 `FrameAssembler` 组帧后上抛。

## 4. 功能边界梳理

1. connection
- 负责初始化、路由、信令解析、状态回调。

2. modules/cc
- `Pacer` 有基础队列和全局包序号逻辑。
- `bwe.cpp` 为空，拥塞控制尚未实现。

3. modules/dtls
- 具备握手、网络数据喂入、应用数据读写。
- 证书校验逻辑存在但实际放空。

4. modules/message
- `ReliableMessageChannel` 集成 KCP。
- `HalfReliableMessageChannel` 当前为空实现。

5. modules/network
- 基于 libuv 封装 UDP socket 与 IOLoop。
- 错误通知链路不完整。

6. modules/p2p
- LAN Endpoint 可用。
- WAN/Relay Endpoint 文件存在但核心逻辑为空。

7. modules/rtp + modules/video + stream
- RTP 基础能力与视频组帧在。
- 视频发送中的拥塞控制、保护、重传策略未闭环。
- 音频发送/接收基本空实现。

## 5. 问题清单（按严重级别）

### 5.1 高风险

1. 未初始化指针调用（可能崩溃）
- `VideoSendStream::network_channel_` 未初始化却在发送路径使用。
- 证据: `src/transport/rtc2/src/stream/video_send_stream.h`, `src/transport/rtc2/src/stream/video_send_stream.cpp`

2. 未初始化指针调用（可能崩溃）
- `VideoReceiveStream::thread_` 未初始化却直接 `thread_->post`。
- 证据: `src/transport/rtc2/src/stream/video_receive_stream.h`, `src/transport/rtc2/src/stream/video_receive_stream.cpp`

3. 消息通道参数透传断裂（未定义行为风险）
- `ConnectionImpl` 设置了 `mtu/sndwnd/rcvwnd`，但 `MessageChannel` 构造 `ReliableMessageChannel::Params` 时未赋值这些字段。
- `ReliableMessageChannel` 又直接使用该参数配置 KCP。
- 证据: `src/transport/rtc2/src/connection/connection_impl.cpp`, `src/transport/rtc2/src/stream/message_channel.cpp`, `src/transport/rtc2/src/modules/message/reliable_message_channel.cpp`

4. 证书校验实际失效（安全风险）
- `verify_cert` 中直接 `*flags = 0`，未校验 `peer_digest`。
- 证据: `src/transport/rtc2/src/modules/dtls/mbed_dtls.cpp`

5. Pacer 处理循环可能未启动（发送停滞风险）
- 仅看到 enqueue 和 process 定义，未发现明确启动入口。
- 证据: `src/transport/rtc2/src/stream/video_send_stream.cpp`, `src/transport/rtc2/src/modules/cc/pacer.cpp`

### 5.2 中风险

1. P2P 只实现 LAN，公网路径不可用
- `add_remote_info` 对 WAN/Relay 直接输出 Unsupported。
- `create_wan_endpoint`、`create_relay_endpoint` 为空。
- 证据: `src/transport/rtc2/src/modules/p2p/p2p.cpp`, `src/transport/rtc2/src/modules/p2p/wan_endpoint.cpp`, `src/transport/rtc2/src/modules/p2p/relay_endpoint.cpp`

2. 音频流功能未完成
- `AudioSendStream::send` 空实现。
- `AudioReceiveStream::onRtpPacket` 空实现，`ssrc()` 直接返回 0。
- 证据: `src/transport/rtc2/src/stream/audio_send_stream.cpp`, `src/transport/rtc2/src/stream/audio_receive_stream.cpp`

3. half-reliable 接口与行为不一致
- `HalfReliableMessageChannel` 发送接收均 `return false`。
- `MessageChannel::sendMessage` 内部强制 `reliable = true`。
- 证据: `src/transport/rtc2/src/modules/message/half_reliable_message_channel.cpp`, `src/transport/rtc2/src/stream/message_channel.cpp`

4. 关闭与错误处理不完整
- `Client::close`/`Server::close` 为空。
- `onNetError` 吞掉错误。
- `onDtlsDisconnected` 仅日志，无上层状态回调。
- 证据: `src/transport/rtc2/src/rtc2.cpp`, `src/transport/rtc2/src/connection/connection_impl.cpp`

### 5.3 低风险

1. 生产路径中存在 `assert(false)`/`LOG(FATAL)`
- 异常输入可能触发进程终止。
- 证据: `src/transport/rtc2/src/connection/connection_impl.cpp`, `src/transport/rtc2/src/modules/video/frame_assembler.cpp`, `src/transport/rtc2/src/modules/network/network_channel.cpp`, `src/transport/rtc2/src/modules/p2p/p2p.cpp`

2. 信令格式可维护性较弱
- 文本拼接 + 空格分割，不利于扩展和兼容。
- 证据: `src/transport/rtc2/src/connection/connection_impl.cpp`

3. 资源管理细节待完善
- `BIO::create/destroy` 存在，析构路径未见对 `bio_in_` 的显式释放调用。
- 证据: `src/transport/rtc2/src/modules/dtls/mbed_dtls.cpp`

## 6. 测试现状与缺口

### 6.1 现有测试

1. Buffer 测试
- 文件: `src/transport/rtc2/src/modules/buffer_tests.cpp`

2. RTP 包测试
- 文件: `src/transport/rtc2/src/modules/rtp/rtp_packet_tests.cpp`

3. 测试注册
- 文件: `src/transport/rtc2/CMakeLists.txt`

### 6.2 缺失测试

1. Connection + P2P + DTLS 端到端集成测试。
2. MessageChannel/KCP 参数与边界测试。
3. 视频组帧乱序/丢包/抖动场景测试。
4. 断线重连、链路切换、状态回调一致性测试。
5. 线程安全与竞态相关压力测试。

## 7. 优先级改进路线图

### P0（立即处理）

1. 修复 `VideoSendStream::network_channel_` 与 `VideoReceiveStream::thread_` 初始化问题。
2. 修复 MessageChannel 参数透传，确保 KCP 配置确定可控。
3. 恢复证书 digest 校验，阻断 MITM 风险。
4. 明确并验证 pacer 发送循环启动路径。

### P0 函数级修复任务清单（可直接排期）

以下任务按“先防崩溃 -> 再保链路 -> 再保安全”的顺序执行。

#### T1. 修复 VideoSendStream 未初始化 `network_channel_`（崩溃风险）

- 目标文件:
  - `src/transport/rtc2/src/stream/video_send_stream.h`
  - `src/transport/rtc2/src/stream/video_send_stream.cpp`
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
- 涉及函数:
  - `VideoSendStream::VideoSendStream`
  - `VideoSendStream::onPcedPacket`
  - `VideoSendStream::protectAndSendPacket`
  - `ConnectionImpl::init`
- 修改要点:
  1. 在 `VideoSendStream::Params` 中显式增加 `NetworkChannel* network_channel`（或等效依赖注入）。
  2. 在 `ConnectionImpl::init` 创建 `VideoSendStream` 时传入 `network_channel_.get()`。
  3. 在 `VideoSendStream::VideoSendStream` 中初始化 `network_channel_`。
  4. 在 `onPcedPacket/protectAndSendPacket` 添加空指针保护与错误日志（避免静默崩溃）。
- 验收标准:
  1. 视频发送路径不再触发野指针/空指针崩溃。
  2. 发送失败时有明确日志并可定位。

#### T2. 修复 VideoReceiveStream 未初始化 `thread_`（崩溃风险）

- 目标文件:
  - `src/transport/rtc2/src/stream/video_receive_stream.h`
  - `src/transport/rtc2/src/stream/video_receive_stream.cpp`
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
- 涉及函数:
  - `VideoReceiveStream::VideoReceiveStream`
  - `VideoReceiveStream::onRtpPacket`
  - `ConnectionImpl::init`
- 修改要点:
  1. 在 `VideoReceiveStream::Params` 中增加 `ltlib::TaskThread* callback_thread`（或在构造函数内自建并托管线程）。
  2. 在 `ConnectionImpl::init` 为每个 `VideoReceiveStream` 注入 `recv_thread_.get()`。
  3. `onRtpPacket` 增加线程指针有效性检查。
- 验收标准:
  1. 接收 RTP 后调用 `thread_->post` 不崩溃。
  2. 视频回调路径在压力收包下稳定运行。

#### T3. 打通 KCP 参数透传（未定义行为风险）

- 目标文件:
  - `src/transport/rtc2/src/stream/message_channel.cpp`
  - `src/transport/rtc2/src/modules/message/reliable_message_channel.h`
  - `src/transport/rtc2/src/modules/message/reliable_message_channel.cpp`
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
- 涉及函数:
  - `MessageChannel::MessageChannel`
  - `ReliableMessageChannel::ReliableMessageChannel`
  - `ConnectionImpl::init`
- 修改要点:
  1. 在 `MessageChannel::MessageChannel` 中将 `params.mtu/sndwnd/rcvwnd` 完整赋值到 `reliable_params`。
  2. 在 `ReliableMessageChannel::ReliableMessageChannel` 增加参数合法性兜底（例如 `mtu` 下界、窗口下界）。
  3. 对 `ikcp_setmtu/ikcp_wndsize` 返回值做失败处理和日志。
- 验收标准:
  1. KCP 初始化参数全部可追踪、可配置。
  2. 非法参数不会导致未定义行为，且有错误日志。

#### T4. 恢复证书指纹校验（MITM 风险）

- 目标文件:
  - `src/transport/rtc2/src/modules/dtls/mbed_dtls.cpp`
  - `src/transport/rtc2/src/modules/dtls/mbed_dtls.h`
- 涉及函数:
  - `MbedDtls::verify_cert`
- 修改要点:
  1. 计算对端证书 SHA-256 后与 `peer_digest_` 严格比对。
  2. 比对成功时设置通过标志，比对失败时设置失败标志并记录安全日志。
  3. 保留对空 `peer_digest_` 的策略开关（建议默认拒绝，或仅在明确开发模式下放行）。
- 验收标准:
  1. 错误证书无法完成握手。
  2. 正确证书可正常握手。
  3. 日志可明确区分“证书不匹配”与“其他握手失败”。

#### T5. 明确并启动 Pacer 处理循环（发送停滞风险）

- 目标文件:
  - `src/transport/rtc2/src/modules/cc/pacer.h`
  - `src/transport/rtc2/src/modules/cc/pacer.cpp`
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
- 涉及函数:
  - `Pacer::process`
  - （新增）`Pacer::start` / `Pacer::stop`（建议）
  - `ConnectionImpl::init`
- 修改要点:
  1. 为 `Pacer` 增加显式启动入口，确保 `process` 首次调度发生。
  2. 在 `ConnectionImpl::init` 完成组件装配后启动 pacer。
  3. 进程退出或连接销毁时停止调度（避免悬挂回调）。
- 验收标准:
  1. `enqueuePackets` 后可稳定触发 `send_func`。
  2. 空队列时不出现忙等或高 CPU 占用。

#### T6. 完善断链回调与错误上报最小闭环（可观测性缺口）

- 目标文件:
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
  - `src/transport/rtc2/src/rtc2.cpp`
- 涉及函数:
  - `ConnectionImpl::onDtlsDisconnected`
  - `ConnectionImpl::onNetError`
  - `Client::close`
  - `Server::close`
- 修改要点:
  1. `onDtlsDisconnected` 按状态机触发 `params_.on_disconnected`（避免只记日志不通知）。
  2. `onNetError` 记录错误码并映射到上层失败/断链回调。
  3. `Client::close` / `Server::close` 至少完成幂等化收尾（停止发送、释放连接引用）。
- 验收标准:
  1. 断链时上层必收到一次且仅一次状态通知。
  2. 主动 close 不残留悬挂任务。

### P0 建议执行顺序

1. T1 + T2（先消除崩溃点）
2. T5（打通视频发送主链路）
3. T3（稳定消息链路参数）
4. T4（恢复安全基线）
5. T6（补齐最小状态闭环）

### P0 最小回归用例

1. 视频单帧发送: 调用发送接口后应看到 `send_func` 被触发且无崩溃。
2. 视频接收入队: 构造 RTP 包触发 `VideoReceiveStream::onRtpPacket`，应可安全投递到线程。
3. KCP 参数生效: 配置 `mtu/sndwnd/rcvwnd` 后日志能打印实际生效值。
4. 证书校验: 正确 digest 握手成功，错误 digest 握手失败。
5. 断链通知: 模拟网络中断后，上层收到断链回调且无重复通知。

### P1（功能补齐）

1. 完成 AudioSend/AudioReceive 的 RTP 收发逻辑。
2. 实现 half-reliable 通道并恢复接口语义。
3. 完成 WAN/Relay Endpoint 逻辑，保证公网可用。

### P2（工程化提升）

1. 用可恢复错误处理替代生产路径中的 FATAL/assert。
2. 结构化 signaling 协议（建议 JSON 或 protobuf）。
3. 增加可观测性指标（RTT、丢包、码率、重传、握手耗时）。
4. 建立弱网与并发回归测试基线。

## 8. 结论

rtc2 模块已经具备基础分层和主流程骨架，但仍存在关键实现缺口与高风险缺陷。建议按 P0 -> P1 -> P2 分阶段推进，先确保“不会崩 + 能连通 + 有基本安全保证”，再补全功能与质量体系。
