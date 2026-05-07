# Changelog

All notable changes to the stdiobus C++ SDK will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-05-07

### Added
- Initial production release of stdiobus C++ SDK
- Two-layer design:
  - `stdiobus::ffi` — Thin C++ wrapper (1:1 over C embed API)
  - `stdiobus::Bus` — Idiomatic C++ facade with RAII
  - `stdiobus::AsyncBus` — Async adaptor with `std::future`
- Inline namespace versioning (`stdiobus::v1`) for ABI stability
- Version macros (`STDIOBUS_VERSION_MAJOR/MINOR/PATCH/STRING`)
- Kernel API compatibility check (`stdiobus::kernel_compatible()`)
- Modern C++17:
  - `std::string_view` for zero-copy string handling
  - `std::chrono` for type-safe durations
  - `std::function` for callbacks
  - `[[nodiscard]]` on error-returning functions
- RAII-based resource management (automatic cleanup)
- Status-style error handling (default, no exceptions)
- Optional exception mode (`STDIOBUS_CPP_EXCEPTIONS=1`)
- Builder pattern via `BusBuilder`
- Callback support:
  - `on_message()` — Message received from workers
  - `on_error()` — Error occurred
  - `on_log()` — Log message (redirect from stderr)
  - `on_worker()` — Worker lifecycle events
  - `on_client_connect()` / `on_client_disconnect()` — Client events (TCP/Unix modes)
- Event loop integration via `poll_fd()`
- Prebuilt `libstdio_bus.a` for all platforms (no kernel build required)
- CMake build system with `find_package()` support
- pkg-config support (`stdiobus.pc`)
- Conan package support

### Testing
- 61 unit tests (types, errors, FFI, Bus, AsyncBus)
- 13 end-to-end tests (real worker processes)
- 11 conformance tests (mirrors kernel e2e scenarios)
- 34 kernel parity tests (direct C function calls from C++)
- 134 kernel unit tests (original test_main.c linked against same library)
- User E2E test (install → find_package → build → run)

### Platform Support
- macOS arm64 (Apple Silicon)
- macOS x86_64
- Linux x86_64 (glibc)
- Linux aarch64 (glibc)

[Unreleased]: https://github.com/stdiobus/stdiobus-cpp/compare/cpp-sdk-v1.0.0...HEAD
[1.0.0]: https://github.com/stdiobus/stdiobus-cpp/releases/tag/cpp-sdk-v1.0.0
