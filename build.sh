#!/usr/bin/sh

exit_if_fail() {
    if [ $? -ne 0 ]; then
        exit -1
    fi
}

cmake_configure() {
    cmake -B build/$build_type -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_INSTALL_PREFIX=install/$build_type
}

cmake_build() {
    cmake --build build/$build_type --parallel 2
    cmake --install build/$build_type
}

cmake_clean() {
    cmake --build build/$build_type --target clean
}

check_build_type() {
    if [ ${build_type,,} = "debug" ]; then
        build_type="Debug"
        echo "Debug"
    elif [ ${build_type,,} = "release" ] || [ ${build_type,,} = "relwithdebinfo" ]; then
        build_type="RelWithDebInfo"
        echo "RelWithDebInfo"
    else
        echo "Please specify target type [ Debug | Release ]"
        exit -1
    fi
}

rtc_fetch() {
    rtc_url="https://github.com/numbaa/rtc-prebuilt/releases/download/v0.5.2/rtc.linux.tar.gz"
    mkdir -p ./transport/rtc/linux
    echo "Fetch $rtc_url"
    wget $rtc_url -O ./third_party/prebuilt/rtc.linux.tar.gz
    echo "Extra rtc.linux.tar.gz"
    tar -xzvf ./third_party/prebuilt/rtc.linux.tar.gz -C ./transport/rtc/linux
}

prebuilt_fetch() {
    libs=(
        "mbedtls https://github.com/numbaa/mbedtls-build/releases/download/v3.2.1-3/mbedtls.linux.v3.2.1-3.tar.gz"
        "sdl https://github.com/numbaa/sdl-build/releases/download/v2.28.4-5/sdl.linux.v2.28.4-5.tar.gz"
        "libuv https://github.com/numbaa/libuv-build/releases/download/v1.44.1-3/libuv.linux.v1.44.1-3.tar.gz"
        "onevpl https://github.com/numbaa/onevpl-build/releases/download/v2023.3.1-2/onevpl.linux.v2023.3.1-2.tar.gz"
        "opus https://github.com/numbaa/opus-build/releases/download/v1.4-2/opus.linux.v1.4-2.tar.gz"
        "g3log https://github.com/numbaa/g3log-build/releases/download/v2.3-4/g3log.linux.v2.3-4.tar.gz"
        "googletest https://github.com/numbaa/googletest-build/releases/download/v1.13.0-2/googletest.linux.v1.13.0-2.tar.gz"
        "ffmpeg https://github.com/numbaa/ffmpeg-build/releases/download/v5.1.3-7/ffmpeg.linux.v5.1.3-7.tar.gz"
        "protobuf https://github.com/numbaa/protobuf-build/releases/download/v3.24.3-2/protobuf.linux.v3.24.3-2.tar.gz"
        "sqlite https://github.com/numbaa/sqlite-build/releases/download/v3.43.1-6/sqlite3.linux.v3.43.1-6.tar.gz"
    )
    mkdir -p ./third_party/prebuilt
    for lib in "${libs[@]}"; do
        item=($lib)
        lib_name=${item[0]}
        lib_url=${item[1]}
        echo "Fetch $lib_url"
        wget $lib_url -O ./third_party/prebuilt/$lib_name.linux.tar.gz
        mkdir -p ./third_party/prebuilt/$lib_name/linux
        echo "Extra $lib_name.linux.tar.gz"
        tar -xzvf ./third_party/prebuilt/$lib_name.linux.tar.gz -C ./third_party/prebuilt/$lib_name/linux
    done
    rtc_fetch
}

prebuilt_clean() {
    rm -rf third_party/prebuilt
    rm -rf transport/rtc

}

print_usage() {
    echo "Usage:"
    echo "    build.sh prebuilt [ fetch | clean ]"
    echo "    build.sh build [ Debug | Release ]"
    echo "    build.sh clean [ Debug | Release ]"
}

if [ "$1" = "prebuilt" ]; then
    if [ "$2" = "fetch" ]; then
        prebuilt_fetch
    elif [ "$2" = "clean" ]; then
        prebuilt_clean
    else
        echo "Please specify prebuilt type [ fetch | clean ]"
        exit -1
    fi
elif [ "$1" = "clean" ]; then
    build_type=$2
    check_build_type
    cmake_clean
elif [ "$1" = "build" ]; then
    build_type=$2
    check_build_type
    cmake_configure
    cmake_build
else
    print_usage
fi