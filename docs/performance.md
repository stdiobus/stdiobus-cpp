# Performance

## Design Principles

The stdiobus C++ SDK is designed for low-overhead process orchestration:

- **Zero-copy message passing** — Messages are passed as `std::string_view` to callbacks without intermediate copies
- **No hidden allocations** in the hot path (step/ingest cycle)
- **Deterministic memory usage** — Bounded by kernel buffer configuration
- **No background threads** — All I/O is driven by explicit `step()` calls

## Complexity

| Operation | Complexity | Notes |
|-----------|-----------|-------|
| `step()` | O(n) | n = number of ready file descriptors |
| `send()` | O(m) | m = message size (copy into kernel buffer) |
| `start()` | O(w) | w = number of workers to spawn |
| `stop()` | O(w) | w = number of workers to terminate |
| `state()` | O(1) | Atomic read |
| `stats()` | O(1) | Struct copy |
| `worker_count()` | O(1) | Counter read |

## Allocation Behavior

| Operation | Allocates | Notes |
|-----------|-----------|-------|
| `Bus()` constructor | Yes | One `Impl` struct + C kernel allocation |
| `start()` | Yes | Worker process spawn (OS-level) |
| `step()` | No | Processes existing buffers |
| `send()` | No | Writes directly to kernel buffer |
| Callback invocation | No | `string_view` points to kernel buffer |
| `stop()` | No | Signals workers, waits |

## Buffer Ownership

- The kernel owns all message buffers
- `std::string_view` in callbacks points to kernel-owned memory
- Callback data is valid only for the duration of the callback
- If you need to retain a message, copy it: `std::string(msg)`

## Recommended Production Flags

```bash
cmake -S . -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_CXX_FLAGS="-O2 -DNDEBUG" \
    -DSTDIOBUS_BUILD_TESTS=OFF \
    -DSTDIOBUS_BUILD_EXAMPLES=OFF
```

## Event Loop Integration

For maximum throughput, integrate with your application's event loop:

```cpp
int fd = bus.poll_fd();
// Add fd to epoll/kqueue
// When fd is readable:
bus.step(std::chrono::milliseconds(0));  // Non-blocking
```

This avoids polling overhead and wakes only when I/O is available.

## Backpressure

When the kernel buffer is full, `send()` returns `ErrorCode::Full`. This is a retryable error. The recommended pattern:

```cpp
auto err = bus.send(msg);
if (err.code() == stdiobus::ErrorCode::Full) {
    // Buffer full — back off, call step() to drain, then retry
    bus.step(std::chrono::milliseconds(10));
    err = bus.send(msg);
}
```

## Limitations

- Maximum message size is bounded by kernel buffer configuration (default: configurable per worker)
- Worker spawn time depends on OS process creation overhead
- `step()` timeout granularity is milliseconds (limited by epoll/kqueue)
