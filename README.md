# Lanthing

[![win-build](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml/badge.svg)](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml)

Lanthing是一款开源的串流/远程控制工具。除了主控端和被控端，它还包含完整的[服务端](https://github.com/pjlt/lanthing-svr)、[中继服务器](https://github.com/pjlt/relay)。

当前支持下列系统：

* Windows 10 或以上
* Linux (alpha阶段，仅支持主控，当前只在ubuntu20.04+x11+vaapi+drm+egl+opengl3.3+amd平台测试过)

安卓端在早期开发中。

## 特性

* 支持Nvidia，AMD，Intel显卡上的H.264、HEVC硬件编解码
* 支持鼠标、键盘、手柄（手柄需先安装[开源驱动](https://github.com/nefarius/ViGEmBus/releases)）
* 支持P2P或中继
* 支持自建服务器以及中继服务器
* 支持端到端加密，数据无论是走P2P还是中继转发，其他人都无法解密

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

上面提到的**安全通道**，Github Releases页面下载的lanthing，使用的是开发者在香港服务器搭的[lanthing-svr](https://github.com/pjlt/lanthing-svr)，走的TLS1.2。你也可以选择自建这个服务器。

## 编译

### 编译环境

Windows:

* Windows 10 或以上
* Git
* Visual Studio 2022
* CMake 3.21 或以上
* Qt 6

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

## 进度

开发者这么久不发版本？是不是在摸鱼！

让我瞅瞅有没有在[磨洋工](https://github.com/orgs/pjlt/projects/1/views/1)。

## 交流

QQ群: 89746161
