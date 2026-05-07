# Architecture

## Overview

The stdio Bus C++ SDK is a layered wrapper over a prebuilt C kernel library (`libstdio_bus.a`). It provides two API surfaces with different trade-offs:

```mermaid
sequenceDiagram
    participant App as Application Code
    participant Async as stdiobus::AsyncBus
    participant Bus as stdiobus::Bus / BusBuilder
    participant FFI as stdiobus::ffi
    participant Kernel as libstdio_bus.a (C kernel)
    participant OS as OS (pipes, epoll/kqueue)

    App->>Async: request_async(msg)
    Async->>Bus: send(msg)
    Bus->>FFI: Handle::ingest()
    FFI->>Kernel: stdio_bus_ingest()
    Kernel->>OS: write(pipe_fd)
    OS-->>Kernel: bytes written
    Kernel-->>FFI: rc
    FFI-->>Bus: int result
    Bus-->>Async: Error
    Async-->>App: std::future<AsyncResult>
```

## Components

### C Kernel (`libstdio_bus.a`)

- Written in C, compiled as a static library
- Manages worker process lifecycle (fork/exec, pipes, signals)
- Implements the event loop (epoll on Linux, kqueue on macOS)
- Handles message routing between host and workers
- Provides a stable C ABI (`stdio_bus_embed.h`)

### FFI Layer (`stdiobus::ffi`)

- 1:1 mapping over C API functions
- Non-owning `Handle` wrapper for `stdio_bus_t*`
- Type conversions (C enums → C++ enums, C structs → C++ structs)
- No resource management (caller owns lifecycle)

### Bus Facade (`stdiobus::Bus`)

- RAII: constructor creates, destructor destroys
- Callback trampolines: converts C function pointers to `std::function`
- Pimpl pattern: `Bus::Impl` hides C API details from public header
- Move-only semantics (non-copyable)
- `[[nodiscard]]` on error-returning methods

### AsyncBus (`stdiobus::AsyncBus`)

- Wraps `Bus` with request/response correlation
- Returns `std::future<AsyncResult>` for each request
- Simple JSON ID extraction for response matching
- Timeout management with `check_timeouts()`

### BusBuilder

- Fluent API for constructing `Bus` with options
- Validates configuration before creating the bus
- Returns a fully-configured `Bus` instance

## Data Flow

```mermaid
sequenceDiagram
    participant App as Application
    participant SDK as stdiobus::Bus
    participant Kernel as C Kernel
    participant Worker as Worker Process

    App->>SDK: send(msg)
    SDK->>Kernel: stdio_bus_ingest()
    Kernel->>Worker: write(pipe_fd)

    Note over Worker: Processes message

    Worker->>Kernel: read(pipe_fd)
    Kernel->>SDK: callback trampoline
    SDK->>App: on_message(msg)

    App->>SDK: step(timeout)
    SDK->>Kernel: stdio_bus_step()
    Kernel->>Kernel: epoll_wait()
    Kernel-->>SDK: events processed
    SDK-->>App: event count
```

## Ownership Model

| Resource | Owner | Cleanup |
|----------|-------|---------|
| `stdio_bus_t*` handle | `Bus::Impl` | `~Bus()` calls `stdio_bus_destroy()` |
| Worker processes | C kernel | Terminated on `stop()` or `~Bus()` |
| Pipe file descriptors | C kernel | Closed on worker termination |
| Callback storage | `Bus::Impl::options` | Destroyed with `Impl` |
| Message buffers | C kernel | Valid only during callback |

## Error Flow

```mermaid
sequenceDiagram
    participant Caller
    participant Bus
    participant Kernel as C Kernel
    participant Err as Error class

    Caller->>Bus: bus.start()
    Bus->>Kernel: stdio_bus_start()
    Kernel-->>Bus: int rc
    Bus->>Err: Error::from_c(rc)
    Err-->>Bus: Error object

    alt Status mode (default)
        Bus-->>Caller: [[nodiscard]] Error
        Note over Caller: check with if(err)
    else Exception mode (STDIOBUS_CPP_EXCEPTIONS=1)
        Caller->>Err: throw_if_error(err)
        Err-->>Caller: throws Exception
    end
```

## Threading Model

- **Single-threaded by design**: One `Bus` instance per thread
- **No internal threads**: All I/O is driven by `step()` calls
- **Callback context**: Callbacks execute on the thread calling `step()`
- **AsyncBus mutex**: Protects the pending request map only

## Extension Points

| Extension | Mechanism |
|-----------|-----------|
| Custom logging | `on_log()` callback |
| Error monitoring | `on_error()` callback |
| Worker lifecycle | `on_worker()` callback |
| Event loop integration | `poll_fd()` for epoll/kqueue |
| Custom transport | TCP/Unix listener modes |

## Design Decisions

| Decision | Rationale |
|----------|-----------|
| Static library | Simpler deployment, no ABI concerns for shared lib |
| Pimpl pattern | Hides C API from public headers, stable ABI |
| No exceptions by default | Deterministic for embedded/system use cases |
| Inline namespace `v1` | Future ABI versioning without breaking existing code |
| Prebuilt kernel binaries | Users don't need to build the C kernel |
| `string_view` in callbacks | Zero-copy, no allocation in hot path |
