# Build Guide

## Prerequisites

- C++17 compiler (GCC 11+, Clang 14+, AppleClang 15+)
- CMake 3.14+
- Platform: Linux x86_64/aarch64 or macOS x86_64/arm64

## Quick Build

```bash
# Clone
git clone https://github.com/stdiobus/stdiobus-cpp.git
cd stdiobus-cpp

# Build (uses prebuilt kernel from prebuilds/)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel

# Run tests
ctest --test-dir build --output-on-failure
```

## Build Scripts

```bash
./scripts/build.sh              # Debug build with tests + examples
./scripts/build.sh --release    # Release build
./scripts/build.sh --no-tests   # Skip test targets
./scripts/build.sh --no-examples # Skip example targets
```

## CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `STDIOBUS_CPP_EXCEPTIONS` | OFF | Enable exception throwing mode |
| `STDIOBUS_BUILD_TESTS` | ON | Build test targets |
| `STDIOBUS_BUILD_EXAMPLES` | ON | Build example targets |
| `STDIOBUS_BUILD_BENCHMARKS` | OFF | Build benchmark targets |
| `STDIOBUS_WARNINGS_AS_ERRORS` | OFF | Treat warnings as errors |
| `STDIOBUS_SANITIZER` | _(empty)_ | Sanitizer: address, undefined, thread |
| `STDIOBUS_INSTALL` | ON | Generate install targets |
| `STDIO_BUS_LIB_DIR` | _(auto)_ | Path to directory containing libstdio_bus.a |

## Build Types

```bash
# Debug (default) — assertions, debug symbols, no optimization
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

# Release — full optimization, no debug info
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# RelWithDebInfo — optimization + debug symbols (recommended for production profiling)
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
```

## Sanitizer Builds

```bash
# AddressSanitizer + UndefinedBehaviorSanitizer
cmake -S . -B build -DSTDIOBUS_SANITIZER=address,undefined -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build

# ThreadSanitizer
cmake -S . -B build-tsan -DSTDIOBUS_SANITIZER=thread -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan
ctest --test-dir build-tsan
```

## Install

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr/local
cmake --build build
sudo cmake --install build
```

After installation, use in your project:

```cmake
find_package(stdiobus CONFIG REQUIRED)
target_link_libraries(your_app PRIVATE stdiobus::stdiobus)
```

## Cross-Compilation

The SDK includes prebuilt kernel binaries for all supported platforms in `prebuilds/`. CMake auto-detects the correct binary based on `CMAKE_SYSTEM_PROCESSOR`.

To use a custom kernel build:

```bash
cmake -S . -B build -DSTDIO_BUS_LIB_DIR=/path/to/kernel/build
```

## Conan

```bash
# Install dependencies and build
conan create . --build=missing

# Or use in a consumer project
conan install . --output-folder=build --build=missing
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=build/conan_toolchain.cmake
cmake --build build
```

## Troubleshooting

### "libstdio_bus.a not found"

The SDK auto-detects prebuilt binaries from `prebuilds/`. If you see this warning:
1. Ensure you cloned with full history (prebuilds are in the repo)
2. Or set `STDIO_BUS_LIB_DIR` to point to a kernel build directory

### macOS: "C++ headers not found"

The build script auto-detects the SDK path. If it fails:
```bash
export SDKROOT=$(xcrun --show-sdk-path)
cmake -S . -B build
```

### Linker errors about pthread

On Linux, ensure `pthread` is linked. The CMakeLists.txt handles this automatically via `PUBLIC` link dependency.
