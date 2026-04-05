# 测试分阶段建设计划（排除 transport/ltproto/third_party）

## 1. 范围与目标

### 1.1 扫描范围
- 已扫描目录：`src/**`
- 明确排除：`src/transport/**`、`ltproto/**`、`third_party/**`
- 参考基线：`src/ltlib/settings_tests.cpp`

### 1.2 目标
- 识别“可直接新增测试”的模块与功能点。
- 识别“需要先改造代码再加测试”的模块与功能点。
- 给出可落地的分阶段实施路线，便于后续逐步提升覆盖率和回归稳定性。

## 2. 当前测试现状

- 现有自研单测入口主要在 `src/ltlib/settings_tests.cpp`。
- `LT_ENABLE_TEST` 打开后通过 CTest 集成执行测试（见 `cmake/tests/tests.cmake` 与 `src/ltlib/CMakeLists.txt` 中 `test_settings`）。
- 当前测试覆盖集中在 SQLite Settings 基础读写，尚未形成跨模块测试矩阵。

## 3. 可直接添加测试（优先落地）

以下功能具备“低外部依赖、逻辑清晰、可快速断言”的特点，可直接按 `settings_tests.cpp` 的方式新增 gtest。

### 3.1 ltlib 模块（高优先级）

1) `src/ltlib/reconnect_interval.cpp`
- `ReconnectInterval::next()`、`reset()` 为纯状态机逻辑。
- 适合做边界测试：首次值、饱和上限、reset 后回到起点。

2) `src/ltlib/transform.cpp`
- `calcMaxInnerRect()` 为纯计算函数。
- 适合做比例、零边界、横竖屏转换、极端尺寸参数测试。

3) `src/ltlib/times.h`（`TimeDelta`/`Timestamp` 运算符）
- 头文件中的比较与算术运算可直接断言。
- 可先测试不依赖真实系统时钟的运算逻辑。

4) `src/ltlib/strings.cpp`（配合 `strings.h`）
- `utf8To16/utf16To8/base64Encode/base64Decode` 适合参数化测试。
- 可覆盖中文、emoji、非法 base64 输入、空串输入。

5) `src/ltlib/settings.cpp`
- 在现有 `settings_tests.cpp` 基础上扩展：并发读写、异常键值类型、文件损坏恢复策略。
- 当前 `UpdateTime` 已有待修复点（测试中已标注），建议以“已知问题”单列用例。

### 3.2 app 模块中可相对独立的逻辑（中优先级）

1) `src/app/views/friendly_error_code.cpp`
- 错误码到文案映射逻辑可测。
- 可验证未知错误码 fallback 行为。

2) `src/app/views/components/access_token_validator.cpp`
- 输入规整逻辑（trim、长度截断、大小写转换、非法字符判定）易测。

说明：这两处依赖 Qt 类型，但属于轻量字符串/映射逻辑，不需要真实 UI 场景即可测试。

### 3.3 inputs/audio 模块中的轻逻辑点（中优先级）

1) `src/inputs/capturer/input_event.cpp`
- 构造函数与 `InputEventType` 对应关系可直接断言。

2) `src/audio/capturer/fake_audio_capturer.cpp`
- 默认参数初始化与行为可测（初始化成功、空采集循环行为）。

## 4. 需要改造后再加测试（中长期）

以下模块并非不能测，而是“当前耦合度较高，直接写单测成本/脆弱性都偏高”。

### 4.1 强平台 API 或系统副作用

1) `src/firewall.cpp`
- 直接调用 COM/防火墙接口，存在系统副作用。
- 建议先抽象 `IFirewallRuleManager`，单测替换为 Fake/Mock。

2) `src/worker/display_setting.cpp`
- 直接依赖 `EnumDisplaySettingsW`。
- 建议提取显示模式查询接口（如 `IDisplayModeProvider`），协商算法与 WinAPI 解耦。

3) `src/app/check_decode_ability.cpp`
- 直接 `CreateProcessW/fork/execv`，跨进程行为难稳定复现。
- 建议把“命令构造与退出码解释”拆到纯函数，进程执行通过接口注入。

### 4.2 网络、线程与生命周期高度耦合

1) `src/client/client.cpp`
- 参数解析、网络连接、传输层、线程生命周期混在同一对象初始化路径。
- 建议先拆出 `ClientOptionParser`（纯函数），再将网络构建过程接口化。

2) `src/service/service.cpp`
- 涉及 `IOLoop`、tcp/pipe client、线程及大量回调。
- 建议分离协议处理器（纯逻辑）和运行时适配层（I/O、线程）。

3) `src/worker/worker*.cpp`、`src/video/**`、`src/plat/**`
- 广泛依赖硬件/图形/音视频栈（D3D/SDL/FFmpeg/编码器）。
- 建议先将状态机、参数协商、错误码映射等可纯化逻辑抽离再测。

### 4.3 UI 代码与 QWidget 直接耦合

1) `src/app/views/main_window/**`
- 大量逻辑直接操作 QWidget 与信号槽。
- 建议引入 Presenter/ViewModel 层，将可断言逻辑从控件操作中抽离。

## 5. 建议改造清单（最小可行）

优先做小改造，快速换取可测性：

1) 时间源注入
- 在 `ltlib` 和依赖超时逻辑模块中引入 `IClock`。
- 让超时、重连、保活相关逻辑可以固定时间推进测试。

2) 系统调用接口化
- 对防火墙、显示模式、进程启动、系统信息查询加 Provider 抽象层。
- 业务逻辑仅依赖接口，平台代码在 Adapter 中实现。

3) 参数解析与业务执行解耦
- 将 `Client::create` 的参数校验和对象构建拆开。
- 先把参数解析提炼为纯函数，立即获得高价值单测面。

4) 协议处理纯化
- 在 `service/client/worker` 中将 protobuf 消息分发逻辑抽出为独立 handler。
- 输入消息 -> 输出动作（或命令对象）可直接单测。

5) UI 逻辑下沉
- 将文案拼接、状态机和页面跳转决策下沉为无 UI 依赖类。

## 6. 分阶段落地计划

### 阶段 A（1~2 周）：快速建立可回归基础
目标：以最小改动将“纯逻辑+轻依赖”覆盖起来。

交付项：
- 新增 `ltlib` 单测：`reconnect_interval`、`transform`、`strings`、`times(TimeDelta)`。
- 扩展 `settings_tests`：补充边界与异常输入用例。
- 新增 `app` 轻逻辑单测：`friendly_error_code`、`access_token_validator`。
- 新增 `inputs/audio` 轻逻辑单测：`input_event`、`fake_audio_capturer`。

验收标准：
- `ctest` 可稳定执行新增测试。
- 关键纯逻辑模块形成基础回归网。

### 阶段 B（2~4 周）：低成本可测性改造
目标：通过接口抽象和拆分，将“目前难测但价值高”的模块转为可单测。

交付项：
- 完成 `ClientOptionParser` 抽取并加参数校验单测。
- `display_setting` 协商逻辑与枚举 API 解耦并补单测。
- `check_decode_ability` 中命令构造/结果解释逻辑独立并补单测。
- `firewall` 引入接口包装，补“不触发系统副作用”的行为测试。

验收标准：
- 新增改造模块的单测不依赖真实系统状态。
- 关键逻辑可在 CI 稳定复现。

### 阶段 C（持续演进）：协议与流程测试
目标：覆盖核心业务流（连接、鉴权、状态变化、错误路径）。

交付项：
- 在 `service/client/worker` 中逐步引入 handler 级别测试。
- 对关键 protobuf 消息流增加“输入消息 -> 状态/动作”断言。
- 逐步补齐回归用例矩阵（成功路径 + 错误路径）。

验收标准：
- 高风险回归（连接状态、参数解析、关键错误码）可通过自动化测试提前发现。

## 7. 推荐测试目录与命名（建议）

- 维持模块内就近放置（与 `settings_tests.cpp` 保持一致）或统一放置到 `src/<module>/tests/`。
- 命名建议：`<feature>_tests.cpp`。
- 用例命名建议：`<模块><场景><期望>`（便于失败定位）。

## 8. 风险与注意事项

- `video/plat/worker` 中大量功能与硬件或平台驱动耦合，短期应优先做“逻辑下沉”而非强行集成测试。
- Qt 文案测试受翻译/locale 影响，建议固定语言环境或仅断言关键结构。
- 对已知缺陷（如 Settings 更新时间）建议先保留“预期失败/待修复”标签，防止长期沉默。

---

本报告作为测试建设路线基线，建议每完成一个阶段后回写本文件：
- 已完成项
- 新暴露的测试阻塞点
- 下一阶段优先级调整
