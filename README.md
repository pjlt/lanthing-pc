# Lanthing
## 编译
1. git clone --recursive https://github.com/pjlt/lanthing-pc.git
2. ./build.ps1 prebuild
3. 两种方式可选：
```
$> ./build.ps1 build [Debug|Release] /path/to/qt/cmake
```
或者：
```
$> cmake -B build/[Debug|Release] --DCMAKE_BUILD_TYPE=[Debug|Release]  -DLT_QT_CMAKE_PATH=/path/to/qt/cmake -DCMAKE_INSTALL_PREFIX=install/[Debug|Release]
$> cmake --build build/[Debug|Release] --config [Debug|Release] --target install
```


## IDE
目前只在VS测试过，理论上其它支持CMake的IDE都可以。无论是用CMake生成sln文件后VS打开，还是直接用VS打开当前目录，都要设置CMake参数`-DLT_QT_CMAKE_PATH=/path/to/qt/cmake`。
