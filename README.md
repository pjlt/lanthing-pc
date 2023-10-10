# Lanthing
[![win-build](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml/badge.svg)](https://github.com/pjlt/lanthing-pc/actions/workflows/win-build.yml)

## Build
```powershell
git clone --recursive https://github.com/pjlt/lanthing-pc.git
./build.ps1 prebuilt fetch
./build.ps1 build Release
```

You may change the default build options:
```powershell
cp options-default.cmake options-user.cmake  # Then modify options-user.cmake
```
