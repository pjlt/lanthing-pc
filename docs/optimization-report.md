# Lanthing-pc optimization report

## Scope
- Runtime/process architecture and connection flow
- Source modules under src, ltproto, and transport
- Build system under CMakeLists.txt and cmake/*

## High-value optimization opportunities

### 1. Split executable by role to reduce binary size and coupling
Current state:
- One executable handles app/service/client/worker using CLI role dispatch.
- Role-based code paths are gated mostly at runtime.

Why optimize:
- Faster startup for each role.
- Smaller link surface and fewer transitive dependencies.
- Better testability and lower regression blast radius.

Suggestion:
- Introduce role-specific targets: lanthing_app, lanthing_service, lanthing_client, lanthing_worker.
- Keep shared code in internal static/object libraries (lt_core, lt_media, lt_transport).
- Keep current single entry only as compatibility launcher when needed.

### 2. Improve process boundary contracts
Current state:
- Service and worker communicate over named pipe plus protobuf messages.
- Command-line argument surface for worker startup is long and string-concatenated.

Why optimize:
- Easier to introduce subtle parameter mismatch bugs.
- Harder to evolve protocol safely.

Suggestion:
- Move worker bootstrap params from CLI string to a versioned protobuf payload over pipe.
- Keep a minimal CLI only for process identity and debug mode.

### 3. Transport abstraction cleanup
Current state:
- TCP/RTC/RTC2 branching appears in multiple modules.
- Manual new/delete and transport-type switches are repeated.

Why optimize:
- Repetition increases maintenance cost.
- Harder to add new transport types.

Suggestion:
- Add a factory returning unique_ptr wrappers with custom deleters for transport client/server.
- Centralize transport selection and codec mapping in one module.

### 4. Better observability by stage
Current state:
- Logging exists, but stage-level timing and correlation IDs are limited in cross-process paths.

Why optimize:
- Hard to diagnose user-reported latency or negotiation failures.

Suggestion:
- Add per-session trace ID propagated in app/service/client/worker messages.
- Add structured timing points: signaling join, transport up, first frame encode/decode, first input roundtrip.

### 5. Narrow platform-specific code spread
Current state:
- Windows-specific logic is distributed across multiple modules.

Why optimize:
- Cross-platform reasoning and CI are harder.

Suggestion:
- Group platform adapters behind explicit interfaces in src/plat and src/inputs.
- Keep conditional compilation at adapter boundary only.

## CMake-specific optimization points (重点)

### A. Replace global compile definitions/includes with target-scoped settings
Current state:
- Extensive use of add_definitions and include_directories in definition scripts.

Risk:
- Definitions leak to every target and can cause unintended side effects.

Suggestion:
- Use target_compile_definitions and target_include_directories for each target/library.
- Put common compile definitions into an INTERFACE target (for example: lt_build_config).

### B. Reduce monolithic source list maintenance
Current state:
- cmake/targets/targets.cmake manually enumerates very large source lists.

Risk:
- Easy to miss files and cause platform-specific drift.

Suggestion:
- Split into per-module CMakeLists (app/service/client/worker/video/audio/inputs/plat/ltlib).
- Build module static libs and link in final targets.
- Keep root target file focused on composition only.

### C. Avoid duplicated deployment/runtime dependency logic
Current state:
- postbuild uses install(CODE ...), deploy_dlls, deploy_qt6, and runtime dependency scanning.

Risk:
- Duplicate copy/deploy paths can conflict and increase packaging time.

Suggestion:
- Consolidate deployment in one path per platform.
- Prefer install(TARGETS ...) plus modern runtime dependency set handling.

Implementation status (2026-04-05):
- In progress on Windows path.
- Replaced manual `file(GET_RUNTIME_DEPENDENCIES)` flow with install-time runtime dependency set handling in CMake helper layer.
- Unified `windeployqt` arguments so post-build and install-time deployment paths share one source of truth.
- Added toggles to separate development-time post-build deployment from install/package-time deployment.

### D. Make protobuf generation explicitly incremental and robust
Current state:
- ltproto generation uses custom command and a long static proto list.

Risk:
- Add/remove proto files requires manual sync.

Suggestion:
- Move proto declarations into grouped lists per domain and include from sub-files.
- Add explicit BYPRODUCTS for generated files where applicable.
- Consider dedicated custom target for proto generation with clear dependencies.

### E. Improve multi-config vs single-config handling
Current state:
- Build type normalization mixes Release and RelWithDebInfo naming.

Risk:
- Can be confusing with multi-config generators on Windows.

Suggestion:
- Use generator expressions with CMAKE_CONFIGURATION_TYPES for multi-config.
- Keep LT_BUILD_TYPE only for artifact naming; avoid affecting logic where not necessary.

### F. Harden dependency discovery and fallback behavior
Current state:
- find_package PATHS point to prebuilt locations directly.

Risk:
- Path rigidity reduces portability and makes local environment setup fragile.

Suggestion:
- Provide an option to switch between prebuilt and system packages.
- Add clear status/error messages for missing dependency roots.
- Centralize dependency root in one cache variable.

### G. Enable tests through first-class CTest integration
Current state:
- tests include is commented out in top-level CMakeLists.

Risk:
- Regression detection depends on manual checks.

Suggestion:
- Re-enable test option path and register unit tests from module targets.
- Add a CI preset to build and run tests.

## Quick wins (1-2 days)
- Introduce target-scoped compile definitions for the main executable and ltproto first.
- Extract transport selection factory to remove repeated switch blocks.
- Add session trace ID and stage timing metrics.
- Re-enable optional test entry in CMake with default OFF.

## Mid-term roadmap (1-2 weeks)
- Refactor monolithic targets.cmake into module-level targets.
- Consolidate deployment logic per platform.
- Split role-based executable composition while preserving current UX.
