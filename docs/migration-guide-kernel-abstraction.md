# Migration Guide: raw_handle() → IKernel Interface

This guide helps existing stdiobus C++ SDK users migrate from the deprecated `raw_handle()` API to the new kernel abstraction layer. If you never called `raw_handle()` directly, your code requires no changes — the public Bus API is fully backward-compatible.

---

## Table of Contents

1. [What Changed and Why](#1-what-changed-and-why)
2. [Impact Assessment](#2-impact-assessment)
3. [Migration Patterns](#3-migration-patterns)
4. [Using kernel_factory() for Custom Backends](#4-using-kernel_factory-for-custom-backends)
5. [Deprecation Timeline](#5-deprecation-timeline)
6. [FAQ](#6-faq)

---

## 1. What Changed and Why

### The Problem

Prior to this release, the SDK coupled directly to `libstdio_bus.a` via FFI calls. Every build required the prebuilt C kernel binary, which created two problems:

1. **vcpkg/Conan distribution** — package managers cannot accept checked-in platform-specific binaries. The SDK was unpublishable as a source-only port.
2. **Testing without workers** — unit tests required the full C kernel and real worker processes, making CI slow and fragile.

### The Solution

An abstract `IKernel` interface now sits between the SDK facade (`Bus`, `AsyncBus`, `BusBuilder`) and the transport implementation:

```
Bus / AsyncBus / BusBuilder
         │
         ▼
    IKernel (abstract interface)
         │
    ┌────┴────┐
    ▼         ▼
CKernel   EchoKernel   (or your custom kernel)
```

- **CKernel** wraps `libstdio_bus.a` — same production behavior as before.
- **EchoKernel** is a pure C++ loopback kernel — no external dependencies, no worker processes.
- **Custom kernels** can implement `IKernel` for alternative transports (gRPC, Redis, etc.).

### What Stays the Same

The entire public API of `Bus`, `AsyncBus`, and `BusBuilder` is unchanged. If you use the SDK through these classes (which is the recommended path), your code compiles and behaves identically without modification.

### What's Deprecated

`Bus::raw_handle()` is deprecated. It previously returned the underlying `stdio_bus_t*` C handle for direct C API calls. With the kernel abstraction, the Bus may not be backed by a C kernel at all, so `raw_handle()` returns `nullptr` for non-CKernel backends.

---

## 2. Impact Assessment

### You Are NOT Affected If

- You use `Bus`, `AsyncBus`, or `BusBuilder` without calling `raw_handle()`
- You use the `stdiobus::ffi` namespace (it remains unchanged as an independent escape hatch)
- You only use callbacks (`on_message`, `on_error`, etc.) for event handling

### You ARE Affected If

- You call `bus.raw_handle()` to get the `stdio_bus_t*` pointer
- You pass that pointer to `stdio_bus_*` C API functions directly
- You use the raw handle for operations not exposed by the Bus class

### Common raw_handle() Use Cases

| Use Case | Migration Path |
|----------|---------------|
| Calling `stdio_bus_stats()` directly | Use `bus.stats()` instead |
| Calling `stdio_bus_worker_count()` | Use `bus.worker_count()` instead |
| Calling `stdio_bus_poll_fd()` | Use `bus.poll_fd()` instead |
| Calling `stdio_bus_register_embedded_worker()` | Use `BusBuilder::kernel()` with CKernel directly (see below) |
| Passing handle to third-party C code | Continue using `raw_handle()` with CKernel until removal (see timeline) |

---

## 3. Migration Patterns

### Pattern 1: Stats and Queries (Direct Replacement)

**Before:**

```cpp
auto bus = stdiobus::BusBuilder()
    .config_path("config.json")
    .build();

bus.start();

// Direct C API call via raw handle
stdio_bus_t* handle = bus.raw_handle();
int workers = stdio_bus_worker_count(handle);
int sessions = stdio_bus_session_count(handle);
stdio_bus_stats_t stats;
stdio_bus_stats(handle, &stats);
```

**After:**

```cpp
auto bus = stdiobus::BusBuilder()
    .config_path("config.json")
    .build();

bus.start();

// Use Bus methods directly — same data, no raw handle needed
int workers = bus.worker_count();
int sessions = bus.session_count();
auto stats = bus.stats();
// stats.messages_in, stats.messages_out, stats.bytes_in, stats.bytes_out
```

### Pattern 2: Poll FD for Event Loop Integration

**Before:**

```cpp
stdio_bus_t* handle = bus.raw_handle();
int fd = stdio_bus_poll_fd(handle);

struct pollfd pfd = {fd, POLLIN, 0};
poll(&pfd, 1, timeout_ms);
if (pfd.revents & POLLIN) {
    bus.step();
}
```

**After:**

```cpp
int fd = bus.poll_fd();  // Returns -1 if not available (e.g., EchoKernel)

if (fd >= 0) {
    struct pollfd pfd = {fd, POLLIN, 0};
    poll(&pfd, 1, timeout_ms);
    if (pfd.revents & POLLIN) {
        bus.step();
    }
} else {
    // Fallback: timer-based stepping (works with any kernel)
    bus.step(std::chrono::milliseconds(timeout_ms));
}
```

### Pattern 3: Embedded Worker Registration

**Before:**

```cpp
auto bus = stdiobus::BusBuilder()
    .config_path("config.json")
    .build();

bus.start();

// Create socketpair and register via raw handle
int fds[2];
socketpair(AF_UNIX, SOCK_STREAM, 0, fds);

stdio_bus_t* handle = bus.raw_handle();
int worker_id = stdio_bus_register_embedded_worker(handle, fds[0], fds[1], "my-pool");
```

**After (typed kernel path — recommended):**

```cpp
#include <stdiobus/c_kernel.hpp>

// Construct CKernel directly for full control
auto kernel = std::make_unique<stdiobus::CKernel>(config);

auto bus = stdiobus::BusBuilder()
    .kernel(std::move(kernel))
    .on_message([](std::string_view msg) { /* ... */ })
    .build();

bus.start();

// Embedded worker registration is not yet exposed on Bus.
// For now, if you need this, use the ffi namespace directly:
// stdiobus::ffi::Handle h("config.json");
// h.register_embedded_worker(fds[0], fds[1], "my-pool");
```

> **Note:** If embedded worker registration through the Bus facade is critical for your use case, file a feature request. The IKernel interface supports it, and a future Bus method may expose it.

### Pattern 4: Testing with EchoKernel (New Capability)

This is a new pattern enabled by the kernel abstraction — not a migration, but a significant improvement for testing:

**Before (required real workers for any test):**

```cpp
// test_my_app.cpp — needed config.json with real worker commands
auto bus = stdiobus::BusBuilder()
    .config_path("test-config.json")  // Must point to real workers
    .on_message([&](auto msg) { received = msg; })
    .build();

bus.start();  // Spawns actual processes
bus.send(test_message);
bus.step(std::chrono::seconds(1));  // Wait for real worker response
```

**After (pure C++ test, no external processes):**

```cpp
#include <stdiobus/echo_kernel.hpp>

// test_my_app.cpp — no config file, no workers, no I/O
auto bus = stdiobus::BusBuilder()
    .kernel_factory(stdiobus::echo_kernel_factory())
    .on_message([&](auto msg) { received = msg; })
    .build();

bus.start();  // Instant, no processes spawned
bus.send(test_message);
bus.step();  // Message echoed back immediately via callback

EXPECT_EQ(received, test_message);  // Deterministic, fast
```

### Pattern 5: Conditional Compilation (Library Authors)

If you distribute a library that depends on stdiobus and want to support both build modes:

```cpp
#include <stdiobus/bus.hpp>

// Works in both modes — default_kernel_factory() picks the right one
auto bus = stdiobus::BusBuilder()
    .config_json(my_config)
    .on_message(handler)
    .build();

// Check what kernel is backing the bus (if you need to know)
if (bus.poll_fd() == -1) {
    // Likely EchoKernel or a kernel without poll support
    // Use timer-based stepping
}
```

---

## 4. Using kernel_factory() for Custom Backends

The kernel abstraction opens the SDK to custom transport implementations. There are two paths:

### Path A: Typed Kernel (Compile-Time Known)

When you know the kernel type at compile time, construct it directly and pass it to the builder. This is the recommended approach — no JSON parsing, no factory indirection.

```cpp
#include <stdiobus/bus.hpp>
#include "my_redis_kernel.hpp"

// Construct with typed config — no JSON needed
auto kernel = std::make_unique<RedisKernel>("redis://localhost:6379", pool_size);

auto bus = stdiobus::BusBuilder()
    .kernel(std::move(kernel))
    .on_message([](std::string_view msg) { /* ... */ })
    .build();

bus.start();
```

Benefits:
- Compile-time type safety
- No JSON serialization/parsing overhead
- No `validate_config()` call by the facade (kernel is valid by construction)
- Clear ownership semantics

### Path B: KernelFactory (Runtime/File-Based Config)

When configuration comes from a file or is determined at runtime, provide a factory:

```cpp
#include <stdiobus/bus.hpp>
#include "my_redis_kernel.hpp"

// Define a factory that creates your kernel from JSON
stdiobus::KernelFactory redis_factory() {
    return [](std::string_view json) -> std::unique_ptr<stdiobus::IKernel> {
        return std::make_unique<RedisKernel>(json);
    };
}

// Use with BusBuilder
auto bus = stdiobus::BusBuilder()
    .kernel_factory(redis_factory())
    .config_json(R"({"endpoint":"redis://localhost:6379","pool_size":4})")
    .on_message([](std::string_view msg) { /* ... */ })
    .build();

if (!bus) {
    // Factory failed or validate_config() rejected the JSON
    std::cerr << "Bus initialization failed" << std::endl;
    return 1;
}
```

The facade will:
1. Call your factory with the JSON config string
2. Call `validate_config()` on the returned kernel
3. Call `set_callbacks()` to wire up event delivery
4. The kernel is then ready for `start()`

### Implementing IKernel

See [kernel-implementor-guide.md](kernel-implementor-guide.md) for a complete walkthrough. The minimal contract:

1. Implement all 16 pure virtual methods from `IKernel`
2. Follow the lifecycle state machine (Created → Running → Stopped)
3. Invoke callbacks only from within `step()`
4. Return `KERNEL_INTERFACE_VERSION` from `interface_version()`
5. Implement `validate_config()` to define your configuration schema

---

## 5. Deprecation Timeline

| Version | Status | Details |
|---------|--------|---------|
| **1.0.0** (current) | `[[deprecated]]` | `raw_handle()` compiles with a warning. Returns `nullptr` for non-CKernel backends. Full functionality preserved when backed by CKernel. |
| **1.1.0** (planned) | `[[deprecated]]` | No behavioral change. Deprecation warning remains. |
| **2.0.0** (future) | **Removed** | `raw_handle()` will be removed from the public API. Code calling it will fail to compile. |

### What to Do Now

1. **Audit your code** for `raw_handle()` calls (search for `raw_handle`).
2. **Replace with Bus methods** where possible (see Pattern 1 and 2 above).
3. **For advanced C API access**, use `stdiobus::ffi::Handle` directly — it remains as an independent, non-deprecated escape hatch for users who need raw C kernel access.
4. **Suppress the warning** temporarily if you cannot migrate yet:

```cpp
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
auto* handle = bus.raw_handle();
#pragma GCC diagnostic pop
```

### The ffi Namespace Remains

`stdiobus::ffi::Handle` (`<stdiobus/ffi.hpp>`) is a thin 1:1 wrapper over the C API. It is NOT deprecated and will continue to exist for users who need direct C kernel access. The difference:

- `Bus` + `IKernel` = kernel-agnostic, high-level, recommended path
- `ffi::Handle` = C kernel only, low-level, escape hatch

---

## 6. FAQ

### Q: Does my existing code still compile?

**Yes.** The Bus, AsyncBus, and BusBuilder public APIs are fully source-compatible. If you don't call `raw_handle()`, you get zero warnings. If you do call it, you get a deprecation warning but it still compiles and works.

### Q: Does behavior change if I don't touch my code?

**No.** When built with `STDIOBUS_ENABLE_C_KERNEL=ON` (the default when `libstdio_bus.a` is present), the default kernel factory returns CKernel, which delegates to the same C API as before. Behavior is bit-for-bit equivalent.

### Q: What happens if I build without libstdio_bus.a?

The SDK builds in pure C++ mode (`STDIOBUS_ENABLE_C_KERNEL=OFF`). The default kernel factory returns EchoKernel. `raw_handle()` returns `nullptr`. Tests that require real workers will need to be skipped or adapted.

### Q: Can I use EchoKernel in production?

EchoKernel is designed for testing and prototyping. It echoes messages back without routing them to workers. It's useful for:
- Unit tests
- Integration tests of your application logic
- CI pipelines without the C kernel binary
- Demos and prototypes

For production with real worker processes, use CKernel (the default when `libstdio_bus.a` is available).

### Q: How do I check which kernel is backing my Bus?

There's no direct method to query the kernel type through Bus. However:
- `bus.poll_fd()` returns `-1` for EchoKernel, a valid fd for CKernel
- `bus.worker_count()` returns `0` for EchoKernel (no real workers)

If you need to know the kernel type, use the typed path (`BusBuilder::kernel()`) where you control construction explicitly.

### Q: What about AsyncBus?

`AsyncBus` operates through `Bus` and is completely unaffected. It continues to work with any kernel backend. No changes needed.

### Q: I need embedded worker registration through Bus. What do I do?

Currently, embedded worker registration is available through:
1. The `IKernel` interface directly (if you hold a reference to the kernel)
2. The `stdiobus::ffi::Handle` escape hatch

If you need this exposed on the Bus class, file a feature request. The underlying infrastructure supports it.

---

## Further Reading

- [kernel-interface-contract.md](kernel-interface-contract.md) — formal IKernel method contracts
- [kernel-implementor-guide.md](kernel-implementor-guide.md) — guide for writing custom kernels
- `include/stdiobus/kernel.hpp` — IKernel interface definition
- `include/stdiobus/echo_kernel.hpp` — EchoKernel (reference minimal implementation)
- `include/stdiobus/c_kernel.hpp` — CKernel (production adapter over libstdio_bus.a)

---

*This document satisfies Requirements 12.3 and 11.3 of the Kernel Abstraction Layer specification.*
