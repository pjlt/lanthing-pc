#!/usr/bin/env sh

exit_if_fail() {
    if [ $? -ne 0 ]; then
        exit -1
    fi
}

cmake_configure() {
    if [ -z "$LT_DUMP_URL" ]; then
        cmake -B build/$build_type -DCMAKE_BUILD_TYPE=$build_type -DCMAKE_INSTALL_PREFIX=install/$build_type
    exit_if_fail
    else
        cmake -B build/$build_type -DCMAKE_BUILD_TYPE=$build_type -DLT_DUMP=ON -DLT_DUMP_URL="$LT_DUMP_URL" -DCMAKE_INSTALL_PREFIX=install/$build_type
    exit_if_fail
    fi
}

cmake_build() {
    cmake --build build/$build_type --parallel 3
    exit_if_fail
    cmake --install build/$build_type
    exit_if_fail
}

cmake_clean() {
    cmake --build build/$build_type --target clean
}

check_build_type() {
    bt=$( tr '[:upper:]' '[:lower:]' <<<"$build_type" )
    if [ ${bt} = "debug" ]; then
        build_type="Debug"
        echo "Debug"
    elif [ ${bt} = "release" ] || [ ${bt} = "relwithdebinfo" ]; then
        build_type="RelWithDebInfo"
        echo "RelWithDebInfo"
    else
        echo "Please specify target type [ Debug | Release ]"
        exit -1
    fi
}

rtc_fetch() {
    rtc_url="https://github.com/numbaa/rtc-prebuilt/releases/download/v0.7.10/rtc.mac.zip"
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
        "ffmpeg https://github.com/numbaa/ffmpeg-build/releases/download/v5.1.3-13/ffmpeg.mac.v5.1.3-13.tar.gz"
        "protobuf https://github.com/numbaa/protobuf-build/releases/download/v3.24.3-9/protobuf.mac.v3.24.3-9.tar.gz"
        "sqlite https://github.com/numbaa/sqlite-build/releases/download/v3.43.1-8/sqlite3.mac.v3.43.1-8.tar.gz"
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
    rtc_fetch
}

make_bundle() {
    mkdir -p install/$build_type/lanthing.app/Contents/Frameworks
    cp third_party/prebuilt/g3log/mac/lib/*.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/protobuf/mac/lib/*.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/ffmpeg/mac/lib/libavutil.57.28.100.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/ffmpeg/mac/lib/libavcodec.59.37.100.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/ffmpeg/mac/lib/libswresample.4.7.100.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/libuv/mac/lib/*.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/sdl/mac/lib/*.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/mbedtls/mac/lib/*.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp third_party/prebuilt/sqlite/mac/*.dylib install/$build_type/lanthing.app/Contents/Frameworks/
    cp install/$build_type/bin/librtc* install/$build_type/lanthing.app/Contents/Frameworks/
    install_name_tool -change ./install/lib/libavcodec.59.dylib @rpath/libavcodec.59.37.100.dylib install/$build_type/lanthing.app/Contents/MacOS/lanthing
    install_name_tool -change ./install/lib/libavutil.57.dylib @rpath/libavutil.57.28.100.dylib install/$build_type/lanthing.app/Contents/MacOS/lanthing
    install_name_tool -change ./install/lib/libswresample.4.dylib @rpath/libswresample.4.7.100.dylib install/$build_type/lanthing.app/Contents/MacOS/lanthing
    install_name_tool -change /Users/runner/work/sqlite-build/sqlite-build/sqlite/build/install/lib/libsqlite3.0.dylib  @rpath/libsqlite3.0.dylib install/RelWithDebInfo/lanthing.app/Contents/MacOS/lanthing
    macdeployqt install/$build_type/lanthing.app
}

prebuilt_clean() {
    rm -rf third_party/prebuilt
    rm -rf transport/rtc
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
    make_bundle
else
    print_usage
fi
