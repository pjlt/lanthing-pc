# Lanthing

[![win-build](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml/badge.svg)](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml)

Lanthing是一款开源的串流/远程控制工具。除了主控端和被控端，它还包含完整的[服务端](https://github.com/pjlt/lanthing-svr)、[中继服务器](https://github.com/pjlt/relay)。

当前只支持Windows，不久的未来会覆盖更多端。

## 特性

* 支持Nvidia，AMD，Intel显卡上的H.264、HEVC硬件编解码
* 支持鼠标、键盘、手柄（手柄需先安装[开源驱动](https://github.com/nefarius/ViGEmBus/releases)）
* 支持P2P或中继
* 支持自建服务器以及中继服务器
* 支持端到端加密，数据无论是走P2P还是中继转发，其他人都无法解密

## 编译

### 编译环境

* Windows 10 或以上
* Git
* Visual Studio 2022
* CMake 3.21 或以上
* Qt 6

### 编译命令

```powershell
git clone --recursive https://github.com/pjlt/lanthing-pc.git
cd lanthing-pc
cp options-default.cmake options-user.cmake  #可选，如果需要修改编译选项则执行此步，并修改相应选项
./build.ps1 prebuilt fetch
./build.ps1 build Release
```

## 运行

1. 下载[Github Releases](https://github.com/pjlt/lanthing-pc/releases)页面下的lanthing.zip并解压
2. 管理员运行app.exe
