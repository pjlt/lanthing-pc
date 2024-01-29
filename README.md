# Lanthing

[![build](https://github.com/pjlt/lanthing-pc/actions/workflows/build.yml/badge.svg)](https://github.com/pjlt/lanthing-pc/actions/workflows/build.yml)

Lanthing是一款开源的串流/远程控制工具。除了主控端和被控端，它还包含完整的[服务端](https://github.com/pjlt/lanthing-svr)、[中继服务器](https://github.com/pjlt/relay)。

当前支持下列系统：

* Windows 10 或以上
* Linux
* Android

## 特性

* 支持Nvidia，AMD，Intel显卡上的H.264、HEVC硬件编解码
* 支持鼠标、键盘、手柄（手柄需先安装[开源驱动](https://github.com/nefarius/ViGEmBus/releases)）
* 支持P2P或中继
* 支持自建服务器以及中继服务器
* 支持端到端加密，数据无论是走P2P还是中继转发，其他人都无法解密
* 局域网下依硬件水平可以低至1帧延迟

各平台已实现特性如下表：

|特性     |Windows|Linux|Android||
|--------|:-----:|:-----:|:-----:|-----|
|主控    |✅|✅|✅|
|被控    |✅|❌|❌|
|硬编    |✅|➖|➖|
|硬解    |✅|✅|✅|Linux当前仅支持部分平台和硬件|
|软编软解|❌|❌|❌|
|AVC    |✅|✅|✅|
|HEVC    |✅|✅|✅|需显卡支持|
|YUV444|❌|❌|❌|
|全屏独占|✅|❌|❌|全屏独占模式在局域网下有时可以表现出0帧延迟|
|物理键鼠|✅|✅|✅|
|物理手柄|✅|✅|✅|
|虚拟键鼠|❌|❌|✅|
|虚拟手柄|❌|❌|✅|
|多指触控|❌|❌|✅|
|绝对位置鼠标|✅|✅|✅|鼠标默认使用该模式|
|相对位移鼠标|✅|✅|❌|某些游戏需要切换至该模式才能正常操作，需使用者按Win+Shift+X手动切换|
|自定义鼠标加速|✅|✅|❌|仅在相对位移模式生效|
|P2P    |✅|✅|✅|
|中继    |✅|✅|✅|
|切换显示器|✅|❌|✅|
|远控旋转屏|✅|❌|✅|Android只做了坐标转换，没有适配手机方向|
|拉伸显示|✅|✅|✅|
|原比例显示|✅|❌|✅|
|禁用鼠标|✅|✅|✅|
|禁用键盘|✅|✅|✅|
|禁用手柄|✅|✅|✅|
|禁传声音|✅|✅|✅|
|显示延迟|✅|❌|✅|
|显示FPS|✅|❌|❌|
|显示丢包|✅|❌|❌|
|显示是否P2P|✅|✅|✅|Windows和Linux在窗口标题栏显示|
|自定义过滤虚拟组网|✅|❌|❌|
|自定义端口映射|✅|✅|❌|允许自定义串流使用的UDP端口范围，需自行在路由器做相应设置|
|操作UAC界面|✅|✅|✅|
|操作登录界面|✅|✅|✅|


## 端到端加密

Lanthing的端到端加密具体是如何实现的：
1. 主控端和被控端在程序内部生成 `key-pair` -> `证书` -> `证书指纹`
2. 主控和被控通过一条**安全通道**交换`证书指纹`
3. 主控和被控通过P2P或中继成功建立底层连接，开始`DTLS`握手，握手过程会互发`证书`
4. 主控和被控收到握手中的`证书`，使用`证书指纹`校验证书（其实就是一个哈希值）
5. 确认不是他人伪造的证书后，使用该证书继续后续`DTLS`流程，握手完成后全程加密

通过上面的流程，可以得出以下事实：
1. 不管是P2P还是中继，截取到流量的中间人不可能解密
2. 服务器拿到的只是一个哈希值，并且流量不从这里走，没有解密这一说法

上面提到的**安全通道**，Github Releases页面下载的lanthing，使用的是作者自己搭的[lanthing-svr](https://github.com/pjlt/lanthing-svr)，走的TLS1.2。你也可以选择自建这个服务器。

## 编译

### 编译环境

Windows:

* Windows 10 或以上
* Git
* Visual Studio 2022
* CMake 3.21 或以上
* Qt 6 (msvc)

Linux:

* Ubuntu 20.04 或以上
* Git
* GCC 10 或以上
* CMake 3.21 或以上
* Qt 6

### 编译命令

```powershell
git clone --recursive https://github.com/pjlt/lanthing-pc.git
cd lanthing-pc
cp options-default.cmake options-user.cmake  #可选，如果需要修改编译选项则执行此步，并修改相应选项；如果QT不在环境变量里，则必须修改LT_QT_CMAKE_PATH
./build.[ps1|sh] prebuilt fetch
./build.[ps1|sh] build Release
```

## 使用

1. 下载[Github Releases](https://github.com/pjlt/lanthing-pc/releases)页面下的lanthing.zip并解压
2. 管理员运行app.exe
3. 输入对方设备码和校验码

注意：**连接上对方默认只有手柄权限，鼠标和键盘权限需要被控方手动给予。** 如果不想每次都这么麻烦，可以在`管理`页面修改某个客户端的默认权限。


## 交流

QQ群: 89746161
