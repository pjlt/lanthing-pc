root_dir=$(cd $(dirname $0);pwd)

CMAKE="cmake"

PROTOC=""

action=""

build_type=""

exit_if_fail() {
    if [ $? -ne 0 ];then
        echo -e "\033[31m sub cmd failed!\033[0m"
        exit -1
    fi
}

build_protobuf() {
    echo -e "\033[32m building google-protobuf\033[0m"
    cd third_party/protobuf;
    mkdir -p build2/; cd build2;
    $CMAKE ../cmake -DBUILD_SHARED_LIBS=ON -Dprotobuf_BUILD_TESTS=OFF
    
    $CMAKE --build . --config Debug
    $CMAKE --install . --config Debug --prefix install/Debug
    
    $CMAKE --build . --config Release
    $CMAKE --install . --config Release --prefix install/Release

    cd $root_dir;
}

clear_protobuf() {
    rm third_party/protobuf/build2/* -rf
    exit_if_fail
}

build_google_test() {
    echo -e "\033[32m building google-test\033[0m"
    cd third_party/googletest;
    mkdir -p build/; cd build;
    $CMAKE ../ -Dgtest_force_shared_crt=ON
    $CMAKE --build . --config Debug
    $CMAKE --install . --config Debug --prefix install/Debug
    
    $CMAKE --build . --config Release
    $CMAKE --install . --config Release --prefix install/Release
    cd $root_dir;
}

clear_google_test() {
    rm third_party/googletest/build/* -rf
    exit_if_fail
}

build_mbedtls() {
    echo -e "\033[32m building MbedTLS\033[0m"
    cd third_party/mbedtls
    mkdir -p build/; cd build;
    $CMAKE .. -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DDISABLE_PACKAGE_CONFIG_AND_INSTALL=OFF
    $CMAKE --build . --config Debug
    $CMAKE --install . --config Debug --prefix install/Debug
    $CMAKE --build . --config Release
    $CMAKE --install . --config Release --prefix install/Release
    cd $root_dir;
}

clear_mbedtls() {
    rm third_party/mbedtls/build/* -rf
    exit_if_fail
}

build_libuv() {
    echo -e "\033[32m building libuv\033[0m"
    cd third_party/libuv
    mkdir -p build/; cd build;
    $CMAKE .. -DLIBUV_BUILD_TESTS=OFF -DLIBUV_BUILD_BENCH=OFF
    $CMAKE --build . --config Debug
    $CMAKE --install . --config Debug --prefix install/Debug
    $CMAKE --build . --config Release
    $CMAKE --install . --config Release --prefix install/Release
    cd $root_dir;
}

clear_libuv() {
    rm third_party/libuv/build/* -rf
    exit_if_fail
}

build_sdl() {
    echo -e "\033[32m building SDL\033[0m"
    cd third_party/SDL
    mkdir -p build/; cd build;
    $CMAKE ..
    $CMAKE --build . --config Debug
    $CMAKE --install . --config Debug --prefix install/Debug
    $CMAKE --build . --config Release
    $CMAKE --install . --config Release --prefix install/Release
    cd $root_dir;
}

clear_sdl() {
    rm third_party/SDL/build/* -rf
    exit_if_fail
}

cmake_project() {
    $CMAKE -B build -DCMAKE_BUILD_TYPE=$build_type
    exit_if_fail
}

cmake_build() {
    $CMAKE --build build --config $build_type
    exit_if_fail
}

check_build_type() {
    if [ "$build_type" == "debug" ] || [ "$build_type" == "Debug" ] || [ "$build_type" == "DEBUG" ] ; then
        build_type="Debug"
        echo -e "\033[32m Debug \033[0m"
    elif [ "$build_type" == "release" ] || [ "$build_type" == "Release" ] || [ "$build_type" == "RELEASE" ] ; then 
        echo -e "\033[32m Release \033[0m"
        build_type="Release"
    else
        echo -e "\033[31m Please specify build type [Debug||Release] \033[0m"
        exit -1
    fi
}

action=$1

if [ "$action" == "prebuild" ]; then
    build_protobuf
    build_google_test
    build_mbedtls
    build_libuv
    build_sdl
   
elif [ "$action" == "clean" ];then
    clear_protobuf
    clear_google_test
    clear_mbedtls
    clear_libuv
    clear_sdl

elif [ "$action" == "build" ];then
    build_type=$2
    check_build_type
    cmake_project
    cmake_build
else
    echo -e "\033[32m usage: [prebuild][build][clean]\033[0m"
fi