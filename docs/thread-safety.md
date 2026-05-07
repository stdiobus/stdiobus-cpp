# Thread Safety

## Summary

| Type | Thread-Safe | Notes |
|------|-------------|-------|
| `Bus` | No | One instance per thread, or synchronize externally |
| `AsyncBus` | Partial | `pump()` must be called from one thread; `request_async()` is thread-safe |
| `BusBuilder` | No | Build on one thread, then move the result |
| `Error` | Yes | Immutable after construction, copyable |
| `Stats` | Yes | Plain struct, copyable |
| Free functions | Yes | `version()`, `kernel_compatible()`, `error_code_name()` |

## Bus

`Bus` instances are **not** thread-safe. All methods on a single `Bus` instance must be called from the same thread (or externally synchronized).

```cpp
// CORRECT: one bus per thread
void thread_a() {
    stdiobus::Bus bus("config_a.json");
    bus.start();
    while (bus.is_running()) bus.step(100ms);
}

void thread_b() {
    stdiobus::Bus bus("config_b.json");
    bus.start();
    while (bus.is_running()) bus.step(100ms);
}
```

```cpp
// INCORRECT: shared bus without synchronization
stdiobus::Bus bus("config.json");

// Thread 1
bus.send(msg1);  // DATA RACE

// Thread 2
bus.step(100ms); // DATA RACE
```

## Callbacks

Callbacks are invoked from the thread that calls `step()`. They execute synchronously within the `step()` call. Do not call `Bus` methods from within a callback (re-entrancy is not supported).

```cpp
bus.on_message([](std::string_view msg) {
    // This runs on the thread calling step()
    // Do NOT call bus.send() here
    process(msg);
});
```

## AsyncBus

`AsyncBus` has a mutex protecting the pending request map:

- `request_async()` — Thread-safe (acquires lock)
- `pump()` — Must be called from one thread (drives the event loop)
- `check_timeouts()` — Thread-safe (acquires lock)

Typical pattern: one thread calls `pump()` in a loop, other threads call `request_async()`.

## Global State

The SDK has no mutable global state. `kernel_compatible()` reads compile-time constants only.

## Recommendations

1. **Simplest**: One `Bus` per thread, no sharing
2. **Event loop**: One `Bus` on the event loop thread, use `poll_fd()` for integration
3. **Multi-threaded sends**: Use `AsyncBus` — pump on one thread, request from many
4. **External sync**: Wrap `Bus` in a mutex if you must share (not recommended for performance)
