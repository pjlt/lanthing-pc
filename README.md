# Lanthing
[![win-build](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml/badge.svg)](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml)

## 编译
1. git clone --recursive https://github.com/pjlt/lanthing-pc.git
2. ./build.ps1 prebuilt fetch
3. 创建一个`options-user.cmake`文件，将`options-default.cmake`的内容拷贝进去，根据自身情况修改其中的默认值。
4. ./build.ps1 build Release

