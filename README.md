# Lanthing
## 编译
1. git clone --recursive https://github.com/pjlt/lanthing-pc.git
2. ./build.ps1 prebuild
3. 创建一个`options-user.cmake`文件，将`options-default.cmake`的内容拷贝进去，根据自身情况修改其中的默认值。
4. 两种方式可选：
```
$> ./build.ps1 build [Debug|Release]
```
或者：
```
$> cmake -B build/[Debug|Release] --DCMAKE_BUILD_TYPE=[Debug|Release] -DCMAKE_INSTALL_PREFIX=install/[Debug|Release]
$> cmake --build build/[Debug|Release] --config [Debug|Release] --target install
```
