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

状态: 已完成。

### P2（工程化提升）

目标: 提升稳定性、可维护性与可观测性，形成“可恢复 + 可演进 + 可回归”的工程化基线。

#### P2 计划拆解

#### P2-T1. 生产路径可恢复错误处理改造

- 目标文件:
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
  - `src/transport/rtc2/src/modules/network/network_channel.cpp`
  - `src/transport/rtc2/src/modules/p2p/p2p.cpp`
  - `src/transport/rtc2/src/modules/video/frame_assembler.cpp`
- 实施要点:
  1. 梳理并替换生产路径上的 `assert(false)`/`LOG(FATAL)`，改为错误码、状态回调或软失败分支。
  2. 统一不可恢复错误与可恢复错误分级，避免把可恢复场景升级为进程终止。
  3. 补齐关键失败路径日志字段（连接 ID、通道类型、远端地址、错误码）。
- 验收标准:
  1. 异常输入与弱网抖动场景下，进程不因断言/FATAL 直接退出。
  2. 上层可收到一致的失败回调，且日志可定位根因。

#### P2-T2. signaling 协议结构化改造

状态: 已完成。

- 落地决策:
  1. 协议格式: JSON（基于 `third_party/nlohmann/json`）。
  2. 版本字段: `v: 1`。
  3. signaling key: 保持 `epinfo`，仅替换 value 载荷格式。
  4. 兼容策略: 不兼容旧空格 KV 格式，旧格式按异常信令软失败处理。
  5. 处理策略: 统一在 `ConnectionImpl` 编解码入口进行解析、字段校验与版本校验；字段缺失、类型错误、版本不匹配、非法值均拒绝并记录原因。

- 目标文件:
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
  - `src/transport/rtc2/src/rtc2.cpp`
  - `src/transport/rtc2/include/rtc2/connection.h`
  - `docs/connection-flow.mmd`（按需更新）
- 实施要点:
  1. 将当前“文本拼接 + 空格分割”信令格式替换为结构化协议（优先 JSON，或按需求使用 protobuf）。
  2. 为信令消息定义版本字段与兼容策略，确保新旧格式可平滑过渡。
  3. 统一编解码与校验入口，明确字段缺失、格式错误、版本不兼容时的处理策略。
- 验收标准:
  1. 新格式信令在主流程（建连、候选交换、状态同步）稳定可用。
  2. 异常信令可被识别并降级处理，不触发未定义行为。

#### P2-T3. 可观测性指标与日志体系补齐

- 目标文件:
  - `src/transport/rtc2/src/connection/connection_impl.cpp`
  - `src/transport/rtc2/src/modules/cc/pacer.cpp`
  - `src/transport/rtc2/src/modules/message/reliable_message_channel.cpp`
  - `src/transport/rtc2/src/modules/dtls/mbed_dtls.cpp`
- 实施要点:
  1. 增加关键指标采集: RTT、丢包率、重传次数、发送/接收码率、DTLS 握手耗时。
  2. 打通周期性统计上报或日志输出，统一维度与采样周期。
  3. 定义连接生命周期内的观测点（建连、稳定传输、断连、重连）。
- 验收标准:
  1. 单连接与多连接场景均可持续输出核心指标，字段定义稳定。
  2. 问题复盘可基于日志还原主要链路状态变化。

#### P2-T4. 弱网与并发回归测试基线建设

- 目标文件:
  - `src/transport/rtc2/CMakeLists.txt`
  - `src/transport/rtc2/src/modules/rtp/rtp_packet_tests.cpp`
  - `src/transport/rtc2/src/modules/buffer_tests.cpp`
  - `docs/test-plan-roadmap.md`
- 实施要点:
  1. 补齐连接、消息、媒体的弱网测试用例（乱序、丢包、抖动、高 RTT、突发拥塞）。
  2. 增加并发与压力回归（多连接并发、长时运行、频繁断连重连）。
  3. 在 CTest 中分层组织 smoke/standard/stress 测试集，明确执行时长与准入门槛。
- 验收标准:
  1. 基础回归在 CI 可稳定运行并输出可追踪报告。
  2. 核心场景具备明确通过阈值与失败判据，支持发布前门禁。

#### P2 建议执行顺序

1. P2-T1（先消除生产路径不可恢复崩溃点）。
2. P2-T2（再统一 signaling 语义与兼容策略）。
3. P2-T3（随后补齐指标与日志，增强诊断能力）。
4. P2-T4（最后固化回归基线，形成持续质量门禁）。

#### P2 最小回归用例

1. 错误恢复: 注入非法包与异常状态，验证无进程级崩溃且状态回调一致。
2. 协议兼容: JSON `epinfo` v1 正常建连；旧 KV signaling 被识别并软失败，错误处理分支正确。
3. 可观测性: 建连到断连全流程产出完整指标，字段无缺失、无突变。
4. 压测回归: 多连接弱网运行 30 分钟，观察线程、内存、句柄、重传率与回调稳定性。

## 8. 结论

rtc2 模块已经具备基础分层和主流程骨架。当前 P0 与 P1 已完成，下一阶段建议聚焦 P2 工程化建设，持续提升稳定性、可维护性与可回归能力。
