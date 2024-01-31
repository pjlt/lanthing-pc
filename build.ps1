$root_dir = (Get-Location).Path

function exit_if_fail {
    if ($LastExitCode -ne 0) {
        Write-Host -ForegroundColor Red Sub cmd failed!
        Exit -1
    }
}

function cmake_configure() {
    Invoke-Expression "cmake -B build/$script:build_type -DCMAKE_BUILD_TYPE=$script:build_type -DCMAKE_INSTALL_PREFIX=install/$script:build_type"
    exit_if_fail
}

function cmake_build() {
    # Github Actions runners only have 2 cores
    Invoke-Expression "cmake --build build/$script:build_type --parallel 2 --config $script:build_type --target install"
    exit_if_fail
}

function cmake_clean() {
    Invoke-Expression "cmake --build build/$script:build_type --target clean"
}

function check_build_type() {
    if ($script:build_type -ieq "debug") {
        $script:build_type = "Debug"
        Write-Host -ForegroundColor Green Debug
    } elseif (($script:build_type -ieq "release") -or ($script:build_type -ieq "RelWithDebInfo")) {
        Write-Host -ForegroundColor Green RelWithDebInfo
        $script:build_type = "RelWithDebInfo"
    } else {
        Write-Host -ForegroundColor Red 'Please specify target type [ Debug | Release ]'
        Exit -1
    }
}

function rtc_fetch() {
    $RtcUri = "https://github.com/numbaa/rtc-prebuilt/releases/download/v0.6.3/rtc.win.zip"
    New-Item -ItemType Directory -ErrorAction SilentlyContinue transport/rtc/win
    echo "Fetch $RtcUri"
    Invoke-WebRequest -Uri $RtcUri -OutFile ./third_party/prebuilt/rtc.win.zip
    Expand-Archive ./third_party/prebuilt/rtc.win.zip -DestinationPath ./transport/rtc/win
}

class BuiltLib {
    [string]$Name
    [string]$Uri

    BuiltLib([string]$n, [string]$u) {
        $this.Uri = $u
        $this.Name = $n
    }
}

function prebuilt_fetch() {
    $libs = @(
        [BuiltLib]::new("mbedtls", "https://github.com/numbaa/mbedtls-build/releases/download/v3.2.1-3/mbedtls.win.v3.2.1-3.zip"),
        [BuiltLib]::new("sdl", "https://github.com/numbaa/sdl-build/releases/download/v2.28.4-5/sdl.win.v2.28.4-5.zip"),
        [BuiltLib]::new("vigemclient", "https://github.com/numbaa/vigemclient-build/releases/download/v1/vigemclient.zip"),
        [BuiltLib]::new("libuv", "https://github.com/numbaa/libuv-build/releases/download/v1.44.1-3/libuv.win.v1.44.1-3.zip"),
        [BuiltLib]::new("onevpl", "https://github.com/numbaa/onevpl-build/releases/download/v2023.3.1-2/onevpl.win.v2023.3.1-2.zip"),
        [BuiltLib]::new("opus", "https://github.com/numbaa/opus-build/releases/download/v1.4-2/opus.win.v1.4-2.zip"),
        [BuiltLib]::new("g3log", "https://github.com/numbaa/g3log-build/releases/download/v2.3-4/g3log.win.v2.3-4.zip"),
        [BuiltLib]::new("googletest", "https://github.com/numbaa/googletest-build/releases/download/v1.13.0-2/googletest.win.v1.13.0-2.zip"),
        [BuiltLib]::new("ffmpeg", "https://github.com/numbaa/ffmpeg-build/releases/download/v5.1.3-6/ffmpeg.win.v5.1.3-6.zip"),
        [BuiltLib]::new("protobuf", "https://github.com/numbaa/protobuf-build/releases/download/v3.24.3-2/protobuf.win.v3.24.3-2.zip")
        [BuiltLib]::new("sqlite", "https://github.com/numbaa/sqlite-build/releases/download/v3.43.1-6/sqlite3.win.v3.43.1-6.zip")
    )

    New-Item -ItemType Directory -ErrorAction SilentlyContinue third_party/prebuilt

    foreach ($lib in $libs) {
        $LibName = $lib.Name
        $LibUri = $lib.Uri
        echo "Fetch $LibUri"
        Invoke-WebRequest -Uri $lib.Uri -OutFile ./third_party/prebuilt/$LibName.win.zip
        # exit_if_fail
        echo "Unzip $LibName.win.zip"
        Expand-Archive ./third_party/prebuilt/$LibName.win.zip -DestinationPath ./third_party/prebuilt/$LibName/win
        # exit_if_fail
    }

    rtc_fetch
}

function prebuilt_clean() {
    Remove-Item -Force -Recurse third_party/prebuilt
    Remove-Item -Force -Recurse transport/rtc
}

function print_usage() {
    Write-Host -ForegroundColor Green 'Usage: '
    Write-Host -ForegroundColor Green '    build.ps1 prebuilt [ fetch | clean ]'
    Write-Host -ForegroundColor Green '    build.ps1 build [ Debug | Release ]'
    Write-Host -ForegroundColor Green '    build.ps1 clean [ Debug | Release ]'
    Exit -1
}

$action = $args[0]

if ($action -eq "prebuilt") {
    if ($args[1] -eq "fetch") {
        prebuilt_fetch
    } elseif ($args[1] -eq "clean") {
        prebuilt_clean
    } else {
        Write-Host -ForegroundColor Red 'Please specify prebuilt type [ fetch | clean ]'
        Exit -1
    }
} elseif ($action -eq "clean") {
    $script:build_type=$args[1]
    check_build_type
    cmake_clean
} elseif ($action -eq "build") {
    $script:build_type=$args[1]
    check_build_type
    cmake_configure
    cmake_build
} else {
    print_usage
}