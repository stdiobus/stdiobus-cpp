# stdiobus — C++ SDK for AI Agent Transport

[![Version](https://img.shields.io/badge/version-1.0.0-brightgreen?style=for-the-badge&logo=semver)](CHANGELOG.md)
[![MCP](https://img.shields.io/badge/protocol-MCP-purple?style=for-the-badge&logo=jsonwebtokens)](https://modelcontextprotocol.io)
[![ACP](https://img.shields.io/badge/protocol-ACP-purple?style=for-the-badge&logo=jsonwebtokens)](https://agentclientprotocol.com)
[![stdioBus](https://img.shields.io/badge/ecosystem-stdio%20Bus-ff4500?style=for-the-badge)](https://github.com/stdiobus)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/17)
[![CMake](https://img.shields.io/badge/CMake-3.14%2B-064F8C?style=for-the-badge&logo=cmake)](https://cmake.org)
[![Conan](https://img.shields.io/badge/conan-2.x-6699CB?style=for-the-badge&logo=conan)](https://conan.io)
[![Platform](https://img.shields.io/badge/platform-Linux%20%7C%20macOS-lightgrey?style=for-the-badge&logo=linux)](https://github.com/stdiobus/stdiobus-cpp)
[![Architecture](https://img.shields.io/badge/arch-x86__64%20%7C%20arm64-blue?style=for-the-badge)](https://github.com/stdiobus/stdiobus-cpp)
[![License](https://img.shields.io/badge/license-Apache--2.0-blue?style=for-the-badge&logo=opensourceinitiative)](LICENSE)
[![CI](https://img.shields.io/github/actions/workflow/status/stdiobus/stdiobus-cpp/ci.yml?style=for-the-badge&logo=githubactions&label=CI)](https://github.com/stdiobus/stdiobus-cpp/actions/workflows/ci.yml)
[![Tests](https://img.shields.io/badge/tests-61%20passing-brightgreen?style=for-the-badge&logo=googletest)](https://github.com/stdiobus/stdiobus-cpp)
[![E2E](https://img.shields.io/badge/e2e-13%20passing-brightgreen?style=for-the-badge&logo=googletest)](https://github.com/stdiobus/stdiobus-cpp)
[![Conformance](https://img.shields.io/badge/conformance-11%20passing-brightgreen?style=for-the-badge&logo=googletest)](https://github.com/stdiobus/stdiobus-cpp)
[![Sanitizers](https://img.shields.io/badge/sanitizers-ASAN%20%7C%20UBSAN%20%7C%20TSAN-orange?style=for-the-badge&logo=llvm)](https://github.com/stdiobus/stdiobus-cpp)
[![ABI](https://img.shields.io/badge/ABI-stable%20v1-success?style=for-the-badge)](docs/architecture.md)
[![RAII](https://img.shields.io/badge/style-RAII-blue?style=for-the-badge&logo=cplusplus)](https://en.cppreference.com/w/cpp/language/raii)

A C++17 SDK for [stdio_bus](https://stdiobus.com) — a process-embedded message bus for AI agent orchestration. Manages child worker processes communicating over stdin/stdout using JSON-RPC (MCP/ACP protocols).

## What It Does

- Spawns and manages worker processes (AI agents)
- Routes JSON-RPC messages between host application and workers
- Three operating modes: Embedded (in-process), TCP listener, Unix socket listener
- Event-loop integration via poll fd for epoll/kqueue/libuv

## What It Does Not Do

- Does not implement JSON-RPC parsing (workers handle protocol logic)
- Does not provide networking (TCP/Unix modes are for local IPC only)
- Does not manage application-level state or sessions beyond routing

## API Layers

| Layer | Header | Purpose |
|-------|--------|---------|
| `stdiobus::Bus` | `<stdiobus/bus.hpp>` | RAII facade, builder pattern, std::chrono, std::function |
| `stdiobus::AsyncBus` | `<stdiobus/async.hpp>` | Promise-based async with std::future |
| `stdiobus::ffi` | `<stdiobus/ffi.hpp>` | Thin 1:1 wrapper over C kernel API |

## Quick Start

```cpp
#include <stdiobus.hpp>
#include <iostream>

int main() {
    stdiobus::Bus bus("config.json");

    bus.on_message([](std::string_view msg) {
        std::cout << "Received: " << msg << std::endl;
    });

    if (auto err = bus.start(); err) {
        std::cerr << "Failed: " << err.message() << std::endl;
        return 1;
    }

    bus.send(R"({"jsonrpc":"2.0","method":"echo","params":{},"id":1})");

    while (bus.is_running()) {
        bus.step(std::chrono::milliseconds(100));
    }
}
```

## Installation

### CMake FetchContent

```cmake
include(FetchContent)
FetchContent_Declare(
    stdiobus
    GIT_REPOSITORY https://github.com/stdiobus/stdiobus-cpp.git
    GIT_TAG v1.0.0
)
FetchContent_MakeAvailable(stdiobus)

target_link_libraries(your_app PRIVATE stdiobus::stdiobus)
```

### CMake find_package (after install)

```cmake
find_package(stdiobus CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE stdiobus::stdiobus)
```

### Conan

```bash
conan install . --build=missing
```

```python
# conanfile.txt
[requires]
stdiobus/1.0.0
```

### From Source

```bash
git clone https://github.com/stdiobus/stdiobus-cpp.git
cd stdiobus-cpp
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

## Build & Test

```bash
./scripts/build.sh              # Debug build (tests + examples)
./scripts/build.sh --release    # Release build
./scripts/test.sh --all         # Run all tests
./scripts/verify.sh             # Full verification pipeline
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `STDIOBUS_CPP_EXCEPTIONS` | OFF | Enable exception throwing mode |
| `STDIOBUS_BUILD_TESTS` | ON | Build test targets |
| `STDIOBUS_BUILD_EXAMPLES` | ON | Build example targets |
| `STDIOBUS_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors (CI) |
| `STDIOBUS_SANITIZER` | _(empty)_ | Enable sanitizer (address, undefined, thread) |

## Error Handling

### Status-style (default, no exceptions)

```cpp
if (auto err = bus.start(); err) {
    std::cerr << err.message() << std::endl;
    if (err.is_retryable()) { /* retry */ }
    return 1;
}
```

### Exception mode (opt-in)

```cpp
// Compile with -DSTDIOBUS_CPP_EXCEPTIONS=1
try {
    stdiobus::throw_if_error(bus.start());
} catch (const stdiobus::Exception& e) {
    std::cerr << e.what() << " [" << static_cast<int>(e.code()) << "]" << std::endl;
}
```

## Thread Safety

- `Bus` instances are **not** thread-safe
- Different `Bus` instances may be used concurrently from different threads
- Callbacks are invoked from the thread calling `step()`
- Global library initialization (`kernel_compatible()`) is thread-safe

## Platform Support

| Platform | Architecture | Status |
|----------|-------------|--------|
| Linux | x86_64 | ✓ Supported |
| Linux | aarch64 | ✓ Supported |
| macOS | x86_64 | ✓ Supported |
| macOS | arm64 (Apple Silicon) | ✓ Supported |
| Windows | — | ✗ Not supported |

## Compiler Support

| Compiler | Minimum Version |
|----------|----------------|
| GCC | 11+ |
| Clang | 14+ |
| AppleClang | 15+ |

## Versioning & Stability

This project follows [Semantic Versioning](https://semver.org/). Public API under `include/stdiobus/` is stable within a major version. ABI compatibility is maintained within the same major version via inline namespace versioning (`stdiobus::v1`).

See [CHANGELOG.md](CHANGELOG.md) for release history.

## Documentation

| Document | Description |
|----------|-------------|
| [Full Documentation](docs/README.md) | Comprehensive guides and API reference |
| [API Cheatsheet](docs/03-cpp-sdk/01-api-cheatsheet.md) | Complete API at a glance |
| [Getting Started](docs/01-getting-started/) | Quickstarts for all modes |
| [Core Concepts](docs/02-core-concepts/) | Lifecycle, callbacks, threading, errors |
| [Integration Patterns](docs/06-integration-patterns/) | Retry, timeout, circuit breaker |

## Examples

| Example | Description |
|---------|-------------|
| [basic.cpp](examples/basic.cpp) | Minimal usage with callbacks |
| [builder.cpp](examples/builder.cpp) | Builder pattern configuration |
| [async.cpp](examples/async.cpp) | Async request/response with futures |
| [runner.cpp](examples/runner.cpp) | Long-running daemon with signal handling |

## Current Limitations

- Windows is not supported (use Docker/WSL)
- ABI compatibility is guaranteed only within the same major version
- Maximum message size is bounded by kernel buffer configuration
- `Bus` is not thread-safe; use one instance per thread or synchronize externally
- `AsyncBus` uses simple JSON ID extraction (does not handle nested JSON)

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for development setup and guidelines.

## Security

See [SECURITY.md](SECURITY.md) for vulnerability reporting.

## License

Apache-2.0 — see [LICENSE](LICENSE) for details.
