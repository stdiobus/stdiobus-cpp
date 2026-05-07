# Error Handling

## Error Model

stdiobus uses a dual-mode error handling strategy:

| Mode | Activation | Mechanism |
|------|-----------|-----------|
| Status (default) | No flags needed | `Error` return values, check with `if (err)` |
| Exception | `-DSTDIOBUS_CPP_EXCEPTIONS=1` | `throw_if_error()` throws `Exception` |

## Error Taxonomy

### Recoverable Errors (Retryable)

| Code | Value | Meaning | Action |
|------|-------|---------|--------|
| `Again` | -2 | Resource temporarily unavailable | Retry after `step()` |
| `Full` | -4 | Buffer full | Back off, drain with `step()`, retry |
| `Timeout` | -20 | Operation timed out | Retry with longer timeout |

### Fatal Errors (Non-Retryable)

| Code | Value | Meaning | Action |
|------|-------|---------|--------|
| `Error` | -1 | Generic error | Log and handle |
| `Eof` | -3 | Worker terminated | Check worker status |
| `NotFound` | -5 | Resource not found | Fix configuration |
| `Invalid` | -6 | Invalid argument | Fix caller code |
| `Config` | -10 | Configuration error | Fix config file |
| `Worker` | -11 | Worker process error | Check worker binary |
| `Routing` | -12 | Message routing failed | Check worker availability |
| `Buffer` | -13 | Buffer error | Check buffer configuration |
| `State` | -15 | Invalid state transition | Fix call sequence |
| `PolicyDenied` | -21 | Policy denied operation | Check permissions |

## Status Mode (Default)

```cpp
#include <stdiobus.hpp>

stdiobus::Bus bus("config.json");

// Check construction
if (!bus) {
    std::cerr << "Failed to create bus" << std::endl;
    return 1;
}

// Check operations
if (auto err = bus.start(); err) {
    std::cerr << "Start failed: " << err.message() << std::endl;
    
    if (err.is_retryable()) {
        // Safe to retry
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (auto retry_err = bus.start(); retry_err) {
            return 1;  // Give up
        }
    } else {
        return 1;  // Fatal
    }
}

// Send with retry
auto send_with_retry = [&](std::string_view msg) -> stdiobus::Error {
    for (int i = 0; i < 3; i++) {
        auto err = bus.send(msg);
        if (!err) return err;  // Success
        if (!err.is_retryable()) return err;  // Fatal
        bus.step(std::chrono::milliseconds(10));  // Drain
    }
    return stdiobus::Error(stdiobus::ErrorCode::Timeout, "Send retries exhausted");
};
```

## Exception Mode

Compile with `-DSTDIOBUS_CPP_EXCEPTIONS=1`:

```cpp
#include <stdiobus.hpp>

try {
    stdiobus::Bus bus("config.json");
    stdiobus::throw_if_error(bus.start());
    
    stdiobus::throw_if_error(bus.send(R"({"jsonrpc":"2.0","method":"test","id":1})"));
    
    while (bus.is_running()) {
        bus.step(std::chrono::milliseconds(100));
    }
} catch (const stdiobus::Exception& e) {
    std::cerr << "stdiobus error: " << e.what() << std::endl;
    std::cerr << "Code: " << static_cast<int>(e.code()) << std::endl;
    std::cerr << "Retryable: " << (e.error().is_retryable() ? "yes" : "no") << std::endl;
}
```

## Error Callback

The `on_error()` callback reports asynchronous errors (worker crashes, routing failures) that occur during `step()`:

```cpp
bus.on_error([](stdiobus::ErrorCode code, std::string_view msg) {
    // This is informational — the bus continues running
    std::cerr << "[" << stdiobus::error_code_name(code) << "] " << msg << std::endl;
    
    if (code == stdiobus::ErrorCode::Worker) {
        // A worker crashed — kernel will auto-restart if configured
    }
});
```

## Design Rationale

- **Status by default**: No hidden control flow, deterministic for system/embedded use
- **Exception opt-in**: Convenient for applications that prefer RAII error propagation
- **`[[nodiscard]]`**: Compiler warns if you ignore an `Error` return value
- **No silent failures**: Every error is either returned or reported via callback
