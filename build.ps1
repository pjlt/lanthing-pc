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
    if ($script:build_type -eq "debug") {
        $script:build_type = "Debug"
        Write-Host -ForegroundColor Green Debug
    } elseif ($script:build_type -eq "release") {
        Write-Host -ForegroundColor Green Release
        $script:build_type = "Release"
    } else {
        Write-Host -ForegroundColor Red 'Please specify target type [ Debug | Release ]'
        Exit -1
    }
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
        [BuiltLib]::new("mbedtls", "https://github.com/numbaa/mbedtls-build/releases/download/v3.2.1-1/mbedtls.zip"),
        [BuiltLib]::new("sdl", "https://github.com/numbaa/sdl-build/releases/download/v2.0.20-1/sdl.zip"),
        [BuiltLib]::new("vigemclient", "https://github.com/numbaa/vigemclient-build/releases/download/v1/vigemclient.zip"),
        [BuiltLib]::new("libuv", "https://github.com/numbaa/libuv-build/releases/download/v1.44.1-1/libuv.zip"),
        [BuiltLib]::new("onevpl", "https://github.com/numbaa/onevpl-build/releases/download/v2023.3.1-1/onevpl.zip"),
        [BuiltLib]::new("opus", "https://github.com/numbaa/opus-build/releases/download/v1.4-1/opus.zip"),
        [BuiltLib]::new("g3log", "https://github.com/numbaa/g3log-build/releases/download/v2.3-1/g3log.zip"),
        [BuiltLib]::new("googletest", "https://github.com/numbaa/googletest-build/releases/download/v1.13.0-1/googletest.zip"),
        [BuiltLib]::new("ffmpeg", "https://github.com/numbaa/ffmpeg-build/releases/download/v5.1.3-3/ffmpeg.zip"),
        [BuiltLib]::new("protobuf", "https://github.com/numbaa/protobuf-build/releases/download/v3.24.3-1/protobuf.zip")
    )

    New-Item -ItemType Directory -ErrorAction SilentlyContinue third_party/prebuilt

    foreach ($lib in $libs) {
        $LibName = $lib.Name
        $LibUri = $lib.Uri
        echo "Fetch $LibUri"
        Invoke-WebRequest -Uri $lib.Uri -OutFile ./third_party/prebuilt/$LibName.zip
        # exit_if_fail
        echo "Unzip $LibName"
        Expand-Archive ./third_party/prebuilt/$LibName.zip -DestinationPath ./third_party/prebuilt/$LibName
        # exit_if_fail
    }
}

function prebuilt_clean() {
    Remove-Item -Force -Recurse third_party/prebuilt
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