# Roadmap

## v1.0.0 (Current)

- ✓ Stable public API (Bus, AsyncBus, BusBuilder, FFI)
- ✓ RAII resource management
- ✓ Status-style and exception-mode error handling
- ✓ Prebuilt kernel binaries for Linux/macOS (x64 + arm64)
- ✓ CMake install + find_package support
- ✓ Conan package recipe
- ✓ Comprehensive test suite (unit, e2e, conformance, kernel parity)
- ✓ Full documentation

## v1.1.0 (In Progress)

- [ ] Kernel abstraction layer (IKernel, EchoKernel, CKernel)
- [ ] Pluggable kernel architecture with typed and JSON config paths
- [ ] vcpkg port submission (enabled by kernel abstraction — pure C++ build, no prebuilts required)
- [ ] Coverage reporting in CI (lcov/codecov)
- [ ] Fuzz testing for message parsing paths
- [ ] Performance benchmarks with Google Benchmark
- [ ] `Bus::send_batch()` for bulk message submission
- [ ] Structured logging callback with severity levels

## v1.2.0 (Planned)

- [ ] Custom kernel implementor certification suite
- [ ] Metrics/observability hooks (message counters, latency histograms)
- [ ] Custom allocator support for zero-allocation hot paths
- [ ] `Bus::wait_for()` convenience method (blocks until state change)
- [ ] Health check API for monitoring integration

## v2.0.0 (Future)

- [ ] Remote kernel support (kernel running in separate process)
- [ ] C++20 coroutine support (`co_await` on async operations)
- [ ] Shared library build option with symbol visibility
- [ ] ABI stability guarantees with abi-compliance-checker
- [ ] Windows native support (named pipes)

## Non-Goals

- Full JSON-RPC implementation (workers handle protocol logic)
- HTTP/WebSocket transport (use a reverse proxy)
- Application-level session management
- Built-in serialization (bring your own JSON library)
- GUI or interactive tooling
