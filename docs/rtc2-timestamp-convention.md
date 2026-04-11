# rtc2 RTP Timestamp 约定

更新时间: 2026-04-11

## 背景

rtc2 当前对音视频 RTP 时间戳采用项目内部约定，而非严格遵循标准 RTP 媒体时钟（例如 Opus 48k 时钟）。

## 约定

1. `video_send_stream.cpp` 与 `audio_send_stream.cpp` 的 RTP timestamp 统一使用 1ms 精度。
2. 实现方式为将微秒时间戳除以 1000 后写入 RTP header timestamp 字段。
3. 音频发送不再根据发送频率或帧间隔累进 timestamp。
4. 该行为是项目约定，明确属于非标准 RTP 协议行为。

## 代码位置

- `src/transport/rtc2/src/stream/video_send_stream.cpp`
- `src/transport/rtc2/src/stream/audio_send_stream.cpp`

## 变更注意

1. 后续若做互通性改造（对接标准 RTP 终端）需重新评估该约定。
2. 若恢复标准 RTP 媒体时钟，需要同时修改音频与视频并补充回归测试。
3. 在未完成完整联调前，不要仅修改单一路径（仅音频或仅视频）的 timestamp 规则。
