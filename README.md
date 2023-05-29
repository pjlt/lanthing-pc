# Lanthing
## 编译
1. git clone --recursive https://github.com/pjlt/lanthing-pc.git
2. cd lanthing-pc && bash ./build.sh prebuild
3.1 右键用VS打开lanthing-pc文件夹，然后在`x64-Release`-`管理配置`中设置CMAKE参数 -DLT_QT_CMAKE_PATH=<qt的cmake目录>，然后在VS中生成。或者也可以手动敲cmake命令`cmake -DLT_QT_CMAKE_PATH=xxx`
(添加qt后，`build.sh build`还没改，暂时不能正常工作) 
3.2 mkdir build;cd build; cmake.exe ../ -DCMAKE_BUILD_TYPE=Debug -DLT_QT_CMAKE_PATH="QT6_SDK_DIR" -DCMAKE_INSTALL_PREFIX=./install

## TODO
1. 完善配置系统与设置系统（需要设计怎么存）
2. 更名`ltproto->proto`，`ltlib->???`....
3. 编写cmake install功能，方便打包和调试
