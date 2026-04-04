## 项目概述
该项目是一个远程控制软件，支持Windows、Linux、macOS做主控端，支持Windows做被控端。

## 限制
- 不允许修改`./third_party`内的文件

## 编译构建
- 构建项目不要直接运行`cmake`命令，也不要使用VSCode的`CMake Tools`，请参考`./docs/build.md`

## 代码风格
- 代码风格遵循递归向上查找的`.clang-format`，未找到则使用`./docs/rules/.clang-format`兜底

## 文档
- 项目文档存放在`./docs`目录下，后续生成新文档、更新文档都在该目录下操作
