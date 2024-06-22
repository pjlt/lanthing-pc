#!/usr/bin/env bash

exit_if_fail() {
    if [ $? -ne 0 ]; then
        exit -1
    fi
}

cmake_configure() {
    if [ -z "$LT_DUMP_URL" ]; then
        cmake -B build/$build_type -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_INSTALL_PREFIX=install/$build_type
    else
        cmake -B build/$build_type -DCMAKE_BUILD_TYPE=$build_type -DLT_DUMP=ON -DLT_DUMP_URL="$LT_DUMP_URL" -DCMAKE_INSTALL_PREFIX=install/$build_type
    fi
}

cmake_build() {
    cmake --build build/$build_type --parallel 3
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
    rtc_url="https://github.com/numbaa/rtc-prebuilt/releases/download/v0.7.9/rtc.mac.zip"
    mkdir -p ./transport/rtc/mac
    echo "Fetch $rtc_url"
    curl -L $rtc_url -o ./third_party/prebuilt/rtc.mac.zip
    echo "Extra rtc.mac.zip"
    unzip ./third_party/prebuilt/rtc.mac.zip -d ./transport/rtc/mac
}

prebuilt_fetch() {
    libs=(
        "mbedtls https://github.com/numbaa/mbedtls-build/releases/download/v3.5.2-2/mbedtls.mac.v3.5.2-2.tar.gz"
        "sdl https://github.com/numbaa/sdl-build/releases/download/v2.28.4-7/sdl.mac.v2.28.4-7.tar.gz"
        "libuv https://github.com/numbaa/libuv-build/releases/download/v1.44.1-5/libuv.mac.v1.44.1-5.tar.gz"
        "opus https://github.com/numbaa/opus-build/releases/download/v1.4-4/opus.mac.v1.4-4.tar.gz"
        "g3log https://github.com/numbaa/g3log-build/releases/download/v2.3-6/g3log.mac.v2.3-6.tar.gz"
        "googletest https://github.com/numbaa/googletest-build/releases/download/v1.13.0-4/googletest.mac.v1.13.0-4.tar.gz"
        "ffmpeg https://github.com/numbaa/ffmpeg-build/releases/download/v5.1.3-11/ffmpeg.mac.v5.1.3-11.tar.gz"
        "protobuf https://github.com/numbaa/protobuf-build/releases/download/v3.24.3-9/protobuf.mac.v3.24.3-9.tar.gz"
        "sqlite https://github.com/numbaa/sqlite-build/releases/download/v3.43.1-7/sqlite3.mac.v3.43.1-7.tar.gz"
    )
    mkdir -p ./third_party/prebuilt
    for lib in "${libs[@]}"; do
        item=($lib)
        lib_name=${item[0]}
        lib_url=${item[1]}
        echo "Fetch $lib_url"
        curl -L $lib_url -o ./third_party/prebuilt/$lib_name.mac.tar.gz
        mkdir -p ./third_party/prebuilt/$lib_name/mac
        echo "Extra $lib_name.mac.tar.gz"
        tar -xzvf ./third_party/prebuilt/$lib_name.mac.tar.gz -C ./third_party/prebuilt/$lib_name/mac
    done
    #rtc_fetch
}

prebuilt_clean() {
    rm -rf third_party/prebuilt
    #rm -rf transport/rtc
}

print_usage() {
    echo "Usage:"
    echo "    build.sh prebuilt [ fetch | clean ]"
    echo "    build.sh build [ Debug | Release ]"
    echo "    build.sh package [ Debug | Release ]"
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
elif [ "$1" = "package" ]; then
    build_type=$2
    check_build_type
    make_appimage
else
    print_usage
fi
