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

1. 已完成: T1/T2/T3/T5/T6。
2. 暂不处理: T4（恢复证书 digest 校验，MITM 风险接受，后续单独排期）。

#### P0 保留项：T4. 恢复证书指纹校验（MITM 风险）

- 状态: 暂不处理（风险已知，需在上线前重新评估）。
- 目标文件:
  - `src/transport/rtc2/src/modules/dtls/mbed_dtls.cpp`
  - `src/transport/rtc2/src/modules/dtls/mbed_dtls.h`
- 涉及函数:
  - `MbedDtls::verify_cert`
- 最小改造点:
  1. 计算并比对对端证书 SHA-256 与 `peer_digest_`。
  2. 比对失败显式拒绝握手，并输出安全日志。
  3. 对空 `peer_digest_` 明确策略（默认拒绝或仅开发模式放行）。

### P1（功能补齐）

目标: 补齐媒体与传输核心缺口，形成“音频可用 + 通道语义一致 + 公网可连通”的功能闭环。

#### P1 计划拆解

#### P1-T1. 音频流 RTP 收发闭环

- 目标文件:
  - `src/transport/rtc2/src/stream/audio_send_stream.cpp`
  - `src/transport/rtc2/src/stream/audio_receive_stream.cpp`
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
- 实施要点:
  1. 实现 `AudioSendStream::send` 的 RTP 打包与发包路径（复用现有网络/DTLS 发送链路）。
  2. 实现 `AudioReceiveStream::onRtpPacket` 解包、时序处理与上抛回调。
  3. 修复 `AudioReceiveStream::ssrc()` 返回固定 0 的问题，保证与会话一致。
- 验收标准:
  1. 单向与双向音频均可稳定收发。
  2. 连续 10 分钟通话无崩溃、无明显卡顿。

#### P1-T2. half-reliable 通道落地

- 目标文件:
  - `src/transport/rtc2/src/modules/message/half_reliable_message_channel.cpp`
  - `src/transport/rtc2/src/stream/message_channel.cpp`
  - `src/transport/rtc2/src/modules/message/*`（必要时新增实现文件）
- 实施要点:
  1. 实现 `HalfReliableMessageChannel` 的发送、接收、重传/过期策略。
  2. 移除 `MessageChannel::sendMessage` 对 `reliable=true` 的强制覆盖。
  3. 补齐可靠与半可靠路由分流、统计与日志字段。
- 验收标准:
  1. 业务可按消息类型选择可靠/半可靠，并按预期生效。
  2. 半可靠消息在弱网下体现“可丢弃、低时延”特征。

#### P1-T3. WAN/Relay 连通能力补齐

- 目标文件:
  - `src/transport/rtc2/src/modules/p2p/p2p.cpp`
  - `src/transport/rtc2/src/modules/p2p/wan_endpoint.cpp`
  - `src/transport/rtc2/src/modules/p2p/relay_endpoint.cpp`
- 实施要点:
  1. 完成 `create_wan_endpoint` 与 `create_relay_endpoint` 核心逻辑。
  2. 打通 `add_remote_info` 中 WAN/Relay 分支，接入候选优先级与回退。
  3. 完善直连失败自动回落到 Relay 的状态机与日志。
- 验收标准:
  1. 公网 NAT 场景可建立连接。
  2. 直连失败时可自动回落 Relay，且上层状态一致。

#### P1 建议执行顺序

1. P1-T1（先补齐音频基础能力）。
2. P1-T2（统一消息语义，避免接口行为偏差）。
3. P1-T3（最后补公网连通，便于联调验证）。

#### P1 最小回归用例

1. 音频回环: 发送端持续推送音频帧，接收端稳定回调，音频时长与丢帧率可观测。
2. 通道语义: 同时发送 reliable 与 half-reliable 消息，验证可靠必达与半可靠可丢行为。
3. 公网联通: 模拟双 NAT 与高延迟环境，验证直连优先与 Relay 回退。
4. 长稳测试: 持续运行 30 分钟，观察线程、内存、句柄与连接状态无异常增长。

### P2（工程化提升）

1. 用可恢复错误处理替代生产路径中的 FATAL/assert。
2. 结构化 signaling 协议（建议 JSON 或 protobuf）。
3. 增加可观测性指标（RTT、丢包、码率、重传、握手耗时）。
4. 建立弱网与并发回归测试基线。

## 8. 结论

rtc2 模块已经具备基础分层和主流程骨架，但仍存在关键实现缺口与高风险缺陷。建议按 P0 -> P1 -> P2 分阶段推进，先确保“不会崩 + 能连通 + 有基本安全保证”，再补全功能与质量体系。
