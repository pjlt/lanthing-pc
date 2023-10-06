$root_dir = (Get-Location).Path

function exit_if_fail {
    if ($LastExitCode -ne 0) {
        Write-Host -ForegroundColor Red Sub cmd failed!
        Exit -1
    }
}

function build_protobuf() {
    Write-Host -ForegroundColor Green building google-protobuf
    Set-Location third_party/protobuf
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build2
    Set-Location build2
    Invoke-Expression "cmake ../ -DBUILD_SHARED_LIBS=ON -Dprotobuf_BUILD_TESTS=OFF -Dprotobuf_WITH_ZLIB=OFF"
    Invoke-Expression "cmake --build . --config Debug"
    Invoke-Expression "cmake --install . --config Debug --prefix install/Debug"
    Invoke-Expression "cmake --build . --config Release"
    Invoke-Expression "cmake --install . --config Release --prefix install/Release"
    Set-Location $root_dir
}

function clear_protobuf {
    Remove-Item -Force -Recurse third_party/protobuf/build2/*
    exit_if_fail
}

function build_google_test() {
    Write-Host -ForegroundColor Green building google-test
    Set-Location third_party/googletest;
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake ../ -Dgtest_force_shared_crt=ON"
    Invoke-Expression "cmake --build . --config Debug"
    Invoke-Expression "cmake --install . --config Debug --prefix install/Debug"
    
    Invoke-Expression "cmake --build . --config Release"
    Invoke-Expression "cmake --install . --config Release --prefix install/Release"
    Set-Location $root_dir
}

function clear_google_test() {
    Remove-Item -Force -Recurse third_party/googletest/build/*
    exit_if_fail
}

function build_mbedtls() {
    Write-Host -ForegroundColor Green building MbedTLS
    Set-Location third_party/mbedtls
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .. -DENABLE_TESTING=OFF -DENABLE_PROGRAMS=OFF -DDISABLE_PACKAGE_CONFIG_AND_INSTALL=OFF"
    Invoke-Expression "cmake --build . --config Debug"
    Invoke-Expression "cmake --install . --config Debug --prefix install/Debug"
    Invoke-Expression "cmake --build . --config Release"
    Invoke-Expression "cmake --install . --config Release --prefix install/Release"
    Set-Location $root_dir
}

function clear_mbedtls() {
    Remove-Item -Force -Recurse third_party/mbedtls/build/*
    exit_if_fail
}

function build_libuv() {
    Write-Host -ForegroundColor Green building libuv
    Set-Location third_party/libuv
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .. -DLIBUV_BUILD_TESTS=OFF -DLIBUV_BUILD_BENCH=OFF"
    Invoke-Expression "cmake --build . --config Debug"
    Invoke-Expression "cmake --install . --config Debug --prefix install/Debug"
    Invoke-Expression "cmake --build . --config Release"
    Invoke-Expression "cmake --install . --config Release --prefix install/Release"
    Set-Location $root_dir
}

function clear_libuv() {
    Remove-Item -Force -Recurse third_party/libuv/build/*
    exit_if_fail
}

function build_sdl() {
    Write-Host -ForegroundColor Green building SDL
    Set-Location third_party/SDL
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .."
    Invoke-Expression "cmake --build . --config Debug"
    Invoke-Expression "cmake --install . --config Debug --prefix install/Debug"
    Invoke-Expression "cmake --build . --config Release"
    Invoke-Expression "cmake --install . --config Release --prefix install/Release"
    Set-Location $root_dir
}

function clear_sdl() {
    Remove-Item -Force -Recurse third_party/SDL/build/*
    exit_if_fail
}

function build_onevpl() {
    Write-Host -ForegroundColor Green building oneVPL
    Set-Location third_party/oneVPL
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .. -DBUILD_DEV=ON -DBUILD_DISPATCHER=ON -DBUILD_TOOLS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PREVIEW=OFF -DINSTALL_EXAMPLE_CODE=OFF -DBUILD_DISPATCHER_ONEVPL_EXPERIMENTAL=ON -DBUILD_TOOLS_ONEVPL_EXPERIMENTAL=OFF -DCMAKE_INSTALL_PREFIX=install/Debug"
    Invoke-Expression "cmake --build . --config Debug --target install"
    Invoke-Expression "cmake .. -DBUILD_DEV=ON -DBUILD_DISPATCHER=ON -DBUILD_TOOLS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_PREVIEW=OFF -DINSTALL_EXAMPLE_CODE=OFF -DBUILD_DISPATCHER_ONEVPL_EXPERIMENTAL=ON -DBUILD_TOOLS_ONEVPL_EXPERIMENTAL=OFF -DCMAKE_INSTALL_PREFIX=install/Release"
    Invoke-Expression "cmake --build . --config Release --target install"
    Set-Location $root_dir
}

function clear_onevpl() {
    Remove-Item -Force -Recurse third_party/oneVPL/build/*
    exit_if_fail
}

function build_opus() {
    Write-Host -ForegroundColor Green building opus
    Set-Location third_party/opus
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .. -DCMAKE_INSTALL_PREFIX=install/Debug"
    Invoke-Expression "cmake --build . --config Debug --target install"
    Invoke-Expression "cmake .. -DCMAKE_INSTALL_PREFIX=install/Release"
    Invoke-Expression "cmake --build . --config Release --target install"
    Set-Location $root_dir
}

function clear_opus() {
    Remove-Item -Force -Recurse third_party/opus/build/*
    exit_if_fail
}

function build_g3log() {
    Write-Host -ForegroundColor Green building g3log
    Set-Location third_party/g3log
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .. -DUSE_DYNAMIC_LOGGING_LEVELS=ON -DENABLE_VECTORED_EXCEPTIONHANDLING=OFF -DENABLE_FATAL_SIGNALHANDLING=OFF -DCMAKE_INSTALL_PREFIX=install/Debug"
    Invoke-Expression "cmake --build . --config Debug --target install"
    Invoke-Expression "cmake .. -DUSE_DYNAMIC_LOGGING_LEVELS=ON -DENABLE_VECTORED_EXCEPTIONHANDLING=OFF -DENABLE_FATAL_SIGNALHANDLING=OFF -DCMAKE_INSTALL_PREFIX=install/Release"
    Invoke-Expression "cmake --build . --config Release --target install"
    Set-Location $root_dir
}

function build_vigemclient() {
    Write-Host -ForegroundColor Green building ViGEmClient
    Set-Location third_party/ViGEmClient
    New-Item -ItemType Directory -ErrorAction SilentlyContinue build
    Set-Location build
    Invoke-Expression "cmake .. -DCMAKE_INSTALL_PREFIX=install/Debug -DViGEmClient_DLL=ON"
    Invoke-Expression "cmake --build . --config Debug --target install"
    Invoke-Expression "cmake .. -DCMAKE_INSTALL_PREFIX=install/Release -DViGEmClient_DLL=ON"
    Invoke-Expression "cmake --build . --config Release --target install"
    Set-Location $root_dir
}

function clear_vigemclient() {
    Remove-Item -Force -Recurse third_party/ViGEmClient/build/*
    exit_if_fail
}

function clear_g3log() {
    Remove-Item -Force -Recurse third_party/g3log/build/*
    exit_if_fail
}

function cmake_project() {
    Invoke-Expression "cmake -B build/$script:build_type -DCMAKE_BUILD_TYPE=$script:build_type -DCMAKE_INSTALL_PREFIX=install/$script:build_type"
    exit_if_fail
}

function cmake_build() {
    Invoke-Expression "cmake --build build/$script:build_type --config $script:build_type --target install"
    exit_if_fail
}

function check_build_type() {
    if ($script:build_type -eq "debug") {
        $script:build_type = "Debug"
        Write-Host -ForegroundColor Green Debug
    } elseif ($script:build_type -eq "release") {
        Write-Host -ForegroundColor Green Release
        $script:build_type = "Release"
    } else {
        Write-Host -ForegroundColor Red 'Please specify build type [Debug|Release]'
        Exit -1
    }
}

$action = $args[0]

if ($action -eq "prebuild") {
    build_protobuf
    build_google_test
    build_mbedtls
    build_libuv
    build_sdl
    build_onevpl
    build_opus
    build_g3log
    build_vigemclient
} elseif ($action -eq "clean") {
    clear_protobuf
    clear_google_test
    clear_mbedtls
    clear_libuv
    clear_sdl
    clear_onevpl
    clear_opus
    clear_g3log
    clear_vigemclient
} elseif ($action -eq "build") {
    $script:build_type=$args[1]
    check_build_type
    cmake_project
    cmake_build
} else {
    Write-Host -ForegroundColor Green 'Usage: '
    Write-Host -ForegroundColor Green '    build.ps1 [prebuild][build][clean]'
    Write-Host -ForegroundColor Green '    build.ps1 prebuild'
    Write-Host -ForegroundColor Green '    build.ps1 build [Debug|Release]'
    Write-Host -ForegroundColor Green '    build.ps1 clean'
}