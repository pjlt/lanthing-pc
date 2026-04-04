# Lanthing-pc build and packaging guide

This document is the single reference for local build verification and packaging.
Run all commands from repository root.

## Build system overview

Top-level CMake entry is in `CMakeLists.txt`:
- Loads `options-user.cmake` or fallback `options-default.cmake`
- Includes module scripts under `cmake/`:
  - `definitions/*`
  - `dependencies/*`
  - `targets/*`
  - `postbuild/*`

Supported wrapper scripts:
- Linux: `build.linux.sh`
- macOS: `build.mac.sh`
- Windows: `build.win.ps1`

## Deployment flow (Windows)

Windows now uses a split deployment strategy to avoid duplicate runtime-copy logic:
- Development-time deployment (post-build): optional, for running directly from build output.
- Install/package-time deployment: preferred path for release artifacts.

Key CMake options:
- `LT_ENABLE_POST_BUILD_DEPLOYMENT` (default `ON`)
- `LT_ENABLE_INSTALL_RUNTIME_DEPLOYMENT` (default `ON`)

Current install-time deployment path uses:
- `install(TARGETS ... RUNTIME_DEPENDENCY_SET ...)`
- `install(RUNTIME_DEPENDENCY_SET ...)`
- shared `windeployqt` argument set for both post-build and install-time execution.

If you want faster local iteration and do not need runnable build output tree, disable post-build deployment:

```powershell
cmake -S . -B build/Debug -DLT_ENABLE_POST_BUILD_DEPLOYMENT=OFF
```

## Fail-fast policy

All wrapper scripts are configured to stop immediately on any failed step.

- Linux/macOS scripts use strict shell mode (`set -euo pipefail`)
- Windows PowerShell script uses:
  - `$ErrorActionPreference = 'Stop'`
  - `Set-StrictMode -Version Latest`
  - `exit_if_fail` checks after external commands

If a command fails, fix the issue and rerun the same command.

## Linux

### Fetch prebuilt dependencies
```bash
./build.linux.sh prebuilt fetch
```

### Build
```bash
./build.linux.sh build Debug
# or
./build.linux.sh build Release
```

### Package (AppImage)
```bash
./build.linux.sh package Release
```

### Clean
```bash
./build.linux.sh clean Release
./build.linux.sh prebuilt clean
```

## macOS

### Fetch prebuilt dependencies
```bash
./build.mac.sh prebuilt fetch
```

### Build
```bash
./build.mac.sh build Debug
# or
./build.mac.sh build Release
```

### Package (app bundle + dmg)
```bash
./build.mac.sh package Release
```

### Clean
```bash
./build.mac.sh clean Release
./build.mac.sh prebuilt clean
```

## Windows (PowerShell)

### Fetch prebuilt dependencies
```powershell
./build.win.ps1 prebuilt fetch
```

### Build
```powershell
./build.win.ps1 build Debug
# or
./build.win.ps1 build Release
```

### Build with tests enabled
```powershell
$Env:LT_ENABLE_TEST = "ON"
./build.win.ps1 build Release
cd ./build/RelWithDebInfo
ctest -C RelWithDebInfo --output-on-failure
```

### Clean
```powershell
./build.win.ps1 clean Release
./build.win.ps1 prebuilt clean
```

## Recommended CI/local verification sequence

1. Fetch prebuilt dependencies
2. Configure + build `Release` via platform script
3. Optional: set `LT_ENABLE_TEST=ON` and run `ctest` in the build directory
4. Run packaging step (Linux/macOS)
5. Ensure artifacts exist under `install/<BuildType>/`

## Platform macro boundary check

To monitor platform-specific macro spread (for example `LT_WINDOWS`, `_WIN32`, `WIN32`) outside
adapter boundaries, run:

```powershell
./docs/check-platform-macro-spread.ps1
```

To enforce CI failure when leaks are detected, run:

```powershell
./docs/check-platform-macro-spread.ps1 -FailOnLeak
```

Current allowed boundary prefixes are maintained in the script and include `src/plat/` and
Windows input adapter files under `src/inputs/executor/win_*`.

## Notes

- Script input accepts `Debug` and `Release` (Release is normalized to `RelWithDebInfo` internally).
- Optional dump upload can be enabled with `LT_DUMP_URL` in build scripts.
- If you want to bypass wrapper scripts, use direct CMake commands, but wrapper scripts are the project standard for build/package workflows.
