## 项目概述
该项目是一个远程控制软件，支持Windows、Linux、macOS做主控端，支持Windows做被控端。

## 项目指令
- 不允许修改`./third_party`内的文件
- 执行`git status`命令无需征求同意

## 编译构建
- `./docs/build.md`

## 代码风格
- 代码风格遵循代码文件所在目录的`.clang-format`
- 如果当前目录没有`.clang-format`，则递归向上级目录查找，直到项目根目录
- 如果仍未找到，则使用`./docs/rules/.clang-format`

## 规则
- 项目规则和文档存放在`./docs`目录下，后续生成新文档、更新文档都在该目录下操作
