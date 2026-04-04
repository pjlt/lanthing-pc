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

### Clean
```powershell
./build.win.ps1 clean Release
./build.win.ps1 prebuilt clean
```

## Recommended CI/local verification sequence

1. Fetch prebuilt dependencies
2. Configure + build `Release` via platform script
3. Run packaging step (Linux/macOS)
4. Ensure artifacts exist under `install/<BuildType>/`

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
