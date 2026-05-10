# Custom Kernel Implementor Guide

This guide walks you through creating a custom `IKernel` implementation for the stdiobus C++ SDK. By the end, you'll have a working kernel that plugs into the SDK facade (`Bus`, `AsyncBus`, `BusBuilder`) and can be distributed independently.

For the formal contract specification (preconditions, postconditions, error codes per method), see [kernel-interface-contract.md](kernel-interface-contract.md). This document is the practical "how to build one" companion.

**Header:** `<stdiobus/kernel.hpp>`  
**Namespace:** `stdiobus::v1` (accessed as `stdiobus::`)  
**Reference implementation:** `EchoKernel` in `<stdiobus/echo_kernel.hpp>`

---

## Table of Contents

1. [Quick Start: Minimal Skeleton](#1-quick-start-minimal-skeleton)
2. [Method Groups Walkthrough](#2-method-groups-walkthrough)
3. [The validate_config() Philosophy](#3-the-validate_config-philosophy)
4. [Registering via BusBuilder](#4-registering-via-busbuilder)
5. [Error Handling Patterns](#5-error-handling-patterns)
6. [State Machine Compliance](#6-state-machine-compliance)
7. [Callback Rules](#7-callback-rules)
8. [Testing Your Kernel](#8-testing-your-kernel)
9. [Common Pitfalls](#9-common-pitfalls)

---

## 1. Quick Start: Minimal Skeleton

Here's a complete, compilable `NullKernel` that satisfies the `IKernel` interface. It does nothing useful but demonstrates the structure every implementation needs:

```cpp
#pragma once

#include <stdiobus/kernel.hpp>

#include <deque>
#include <string>

namespace myproject {

class NullKernel final : public stdiobus::IKernel {
public:
    NullKernel() = default;
    ~NullKernel() override = default;

    // Non-copyable, movable
    NullKernel(const NullKernel&) = delete;
    NullKernel& operator=(const NullKernel&) = delete;
    NullKernel(NullKernel&&) noexcept = default;
    NullKernel& operator=(NullKernel&&) noexcept = default;

    // ===== Metadata =====

    int interface_version() const noexcept override {
        return stdiobus::KERNEL_INTERFACE_VERSION;
    }

    std::string_view name() const noexcept override {
        return "null_kernel";
    }

    // ===== Configuration Validation =====

    [[nodiscard]] stdiobus::Error validate_config(std::string_view json) const override {
        // Define YOUR schema here. This is the type of your config.
        // For a null kernel, accept anything:
        return stdiobus::Error::ok();
    }

    // ===== Callbacks =====

    void set_callbacks(const stdiobus::KernelCallbacks& callbacks) override {
        callbacks_ = callbacks;
    }

    // ===== Lifecycle =====

    [[nodiscard]] stdiobus::Error start() override {
        if (state_ != stdiobus::State::Created) {
            return stdiobus::Error(stdiobus::ErrorCode::State,
                                   "start() requires Created state");
        }
        state_ = stdiobus::State::Running;
        return stdiobus::Error::ok();
    }

    int step(int /*timeout_ms*/) override {
        // Process pending work here. Return number of events processed.
        return 0;
    }

    [[nodiscard]] stdiobus::Error stop(int /*timeout_sec*/) override {
        if (state_ != stdiobus::State::Running) {
            return stdiobus::Error(stdiobus::ErrorCode::State,
                                   "stop() requires Running state");
        }
        state_ = stdiobus::State::Stopped;
        return stdiobus::Error::ok();
    }

    // ===== Messaging =====

    [[nodiscard]] stdiobus::Error ingest(const char* message, size_t len) override {
        if (state_ != stdiobus::State::Running) {
            return stdiobus::Error(stdiobus::ErrorCode::State,
                                   "ingest() requires Running state");
        }
        if (!message || len == 0) {
            return stdiobus::Error(stdiobus::ErrorCode::Invalid,
                                   "null or empty message");
        }
        // Route the message to your backend here.
        // For now, just count it:
        stats_.messages_in++;
        stats_.bytes_in += len;
        return stdiobus::Error::ok();
    }

    // ===== State Queries =====

    stdiobus::State state() const noexcept override { return state_; }
    int worker_count() const noexcept override { return 0; }
    int session_count() const noexcept override { return 0; }
    int pending_count() const noexcept override { return 0; }
    int client_count() const noexcept override { return 0; }
    int poll_fd() const noexcept override { return -1; }
    stdiobus::Stats stats() const noexcept override { return stats_; }

    // ===== Embedded Workers =====

    int register_embedded_worker(int /*fd_to_worker*/,
                                 int /*fd_from_worker*/,
                                 std::string_view /*pool_id*/) override {
        // Return negative error code if not supported
        return static_cast<int>(stdiobus::ErrorCode::Invalid);
    }

    [[nodiscard]] stdiobus::Error unregister_embedded_worker(int /*worker_id*/) override {
        return stdiobus::Error(stdiobus::ErrorCode::Invalid,
                               "embedded workers not supported");
    }

private:
    stdiobus::State state_ = stdiobus::State::Created;
    stdiobus::KernelCallbacks callbacks_{};
    stdiobus::Stats stats_{};
};

}  // namespace myproject
```

This compiles, links, and passes the facade's construction checks. From here, you fill in the actual transport logic.

---

## 2. Method Groups Walkthrough

The 16 pure virtual methods fall into six groups. Implement them in this order:

### 2.1 Metadata (implement first)

```cpp
int interface_version() const noexcept override;
std::string_view name() const noexcept override;
```

- `interface_version()` — return `stdiobus::KERNEL_INTERFACE_VERSION` (currently `1`). The facade rejects kernels reporting a version higher than its own.
- `name()` — return a static string identifying your kernel (e.g., `"redis_kernel"`, `"grpc_transport"`). Must remain valid for the kernel's lifetime.

### 2.2 Configuration Validation

```cpp
[[nodiscard]] Error validate_config(std::string_view json) const override;
```

This is where you define what configuration your kernel accepts. See [Section 3](#3-the-validate_config-philosophy) for the full philosophy.

### 2.3 Callbacks

```cpp
void set_callbacks(const KernelCallbacks& callbacks) override;
```

Store the callbacks struct. The facade calls this before `start()`. Individual fields may be empty — always check before invoking. See [Section 7](#7-callback-rules).

### 2.4 Lifecycle

```cpp
[[nodiscard]] Error start() override;
int step(int timeout_ms) override;
[[nodiscard]] Error stop(int timeout_sec) override;
```

- `start()` — initialize your transport (connect, spawn processes, bind sockets). Transition from `Created` to `Running`.
- `step()` — pump I/O. This is your event loop tick. Deliver messages via callbacks here. Return the number of events processed, or a negative error code on fatal failure.
- `stop()` — graceful shutdown. Drain queues, terminate connections, clean up. Transition to `Stopped`.

### 2.5 Messaging

```cpp
[[nodiscard]] Error ingest(const char* message, size_t len) override;
```

Accept a message from the host for routing to your backend. The kernel copies the data internally — the caller may free the buffer after `ingest()` returns.

### 2.6 State Queries

```cpp
State state() const noexcept override;
int worker_count() const noexcept override;
int session_count() const noexcept override;
int pending_count() const noexcept override;
int client_count() const noexcept override;
int poll_fd() const noexcept override;
Stats stats() const noexcept override;
```

All are `const noexcept`, callable in any state, and must never fail. Return `0` or `-1` (for `poll_fd()`) if a concept doesn't apply to your kernel.

### 2.7 Embedded Workers

```cpp
int register_embedded_worker(int fd_to_worker, int fd_from_worker, std::string_view pool_id) override;
[[nodiscard]] Error unregister_embedded_worker(int worker_id) override;
```

If your kernel doesn't support embedded workers (most custom kernels won't), return `ErrorCode::Invalid`. If you do support them, take ownership of the file descriptors and add them to your event loop.

---

## 3. The validate_config() Philosophy

This is the most important design concept to understand:

> **The validator IS the type.**

There is no shared `KernelConfig` struct. Configuration crosses the SDK boundary as a raw JSON string (`std::string_view`). Your `validate_config()` method defines what JSON is acceptable for your kernel. It is the type definition of your configuration.

### What this means in practice

```cpp
[[nodiscard]] Error validate_config(std::string_view json) const override {
    // YOU decide what's valid. This IS your config schema.
    
    if (json.empty()) {
        return Error(ErrorCode::Config, "MyKernel requires configuration");
    }
    
    // Parse and check required fields
    // (use your preferred JSON parser internally)
    auto parsed = my_json_parse(json);
    if (!parsed) {
        return Error(ErrorCode::Config, "Malformed JSON");
    }
    if (!parsed.has("endpoint")) {
        return Error(ErrorCode::Config, "Missing required field 'endpoint'");
    }
    if (!parsed["endpoint"].is_string()) {
        return Error(ErrorCode::Config, "'endpoint' must be a string");
    }
    
    return Error::ok();
}
```

### Key rules

1. **`const` method** — it does not modify kernel state. It's a pure validation check.
2. **Called before `start()`** — the facade calls `validate_config()` and aborts if it fails. If validation passes, `start()` should not fail due to config issues.
3. **You own the parsing** — use whatever JSON library you want internally. The SDK has no opinion on your parser.
4. **Identity validation is valid** — if your kernel needs no config (like EchoKernel), just return `Error::ok()` always.
5. **Be descriptive on failure** — return `ErrorCode::Config` with a message explaining what's wrong. Users see this message.

### The guarantee

If `validate_config()` returns `Error::ok()`, the facade trusts that `start()` will not fail due to configuration problems. Your kernel is making a promise: "I understand this config and can work with it."

---

## 4. Registering via BusBuilder

There are two paths to provide your kernel to the SDK:

### Path A: Typed kernel (recommended)

Pre-construct your kernel and hand it directly to the builder. No factory needed, no JSON validation by the facade — your kernel is already valid by construction.

```cpp
#include <stdiobus/bus.hpp>
#include "my_kernel.hpp"

// Construct your kernel with whatever config it needs
auto kernel = std::make_unique<MyKernel>("redis://localhost:6379", options);

// Hand it to the builder
auto bus = stdiobus::BusBuilder()
    .kernel(std::move(kernel))
    .on_message([](std::string_view msg) {
        // handle messages
    })
    .build();
```

When you use the typed path:
- The facade does NOT call `validate_config()`
- The facade does NOT invoke any factory
- Your kernel must be in `Created` state, ready for `set_callbacks()` + `start()`

### Path B: KernelFactory (JSON path)

Provide a factory function that creates your kernel from a JSON string. Use this when configuration comes from a file or is determined at runtime.

```cpp
#include <stdiobus/bus.hpp>
#include "my_kernel.hpp"

// Define your factory
stdiobus::KernelFactory my_factory() {
    return [](std::string_view json_config) -> std::unique_ptr<stdiobus::IKernel> {
        // Parse config, construct kernel
        auto kernel = std::make_unique<MyKernel>(json_config);
        return kernel;
    };
}

// Register with builder
auto bus = stdiobus::BusBuilder()
    .kernel_factory(my_factory())
    .config_json(R"({"endpoint": "redis://localhost:6379"})")
    .on_message([](std::string_view msg) { /* ... */ })
    .build();
```

When you use the factory path:
- The facade calls your factory with the JSON config string
- Then calls `validate_config()` on the returned kernel
- If validation fails, the Bus is in an invalid state (`operator bool()` returns false)

### Which path to choose

| Scenario | Path |
|----------|------|
| You know the kernel type at compile time | Typed (Path A) |
| Config comes from a file at runtime | Factory (Path B) |
| You want the facade to validate config | Factory (Path B) |
| You construct the kernel with non-JSON config | Typed (Path A) |
| Plugin/dynamic loading | Factory (Path B) |

---

## 5. Error Handling Patterns

### Which ErrorCode to return when

| Situation | ErrorCode | Example |
|-----------|-----------|---------|
| Method called in wrong state | `State` | `ingest()` before `start()` |
| Config validation failed | `Config` | Missing required field |
| Null pointer or zero-length input | `Invalid` | `ingest(nullptr, 0)` |
| Unsupported operation | `Invalid` | `register_embedded_worker()` on a kernel that doesn't support it |
| Internal buffer full (retryable) | `Full` | Message queue at capacity |
| No worker available for routing | `Routing` | All workers busy/dead |
| Worker spawn/communication failure | `Worker` | Process failed to start |
| Operation timed out (retryable) | `Timeout` | `stop()` exceeded timeout |
| Referenced resource not found | `NotFound` | Unknown worker_id in `unregister_embedded_worker()` |

### Pattern: Guard every state-dependent method

```cpp
[[nodiscard]] Error start() override {
    if (state_ != State::Created) {
        return Error(ErrorCode::State, "start() requires Created state");
    }
    // ... actual work ...
    state_ = State::Running;
    return Error::ok();
}

[[nodiscard]] Error ingest(const char* message, size_t len) override {
    if (state_ != State::Running) {
        return Error(ErrorCode::State, "ingest() requires Running state");
    }
    if (!message || len == 0) {
        return Error(ErrorCode::Invalid, "null or empty message");
    }
    // ... actual work ...
    return Error::ok();
}
```

### Pattern: step() returns negative on fatal error

```cpp
int step(int timeout_ms) override {
    if (state_ != State::Running && state_ != State::Stopping) {
        return static_cast<int>(ErrorCode::State);  // negative
    }
    
    int events = 0;
    // ... process I/O, increment events ...
    
    if (fatal_error_occurred) {
        // Also report via callback
        if (callbacks_.on_error) {
            callbacks_.on_error(ErrorCode::Error, "connection lost");
        }
        return static_cast<int>(ErrorCode::Error);  // negative
    }
    
    return events;  // >= 0 on success
}
```

---

## 6. State Machine Compliance

Your kernel must follow this state machine exactly:

```
Created ──start()──→ Running ──stop()──→ Stopped
```

(Internally, `Starting` and `Stopping` are transient states during `start()` and `stop()`.)

### Compliance Checklist

- [ ] Initial state is `Created` after construction
- [ ] `start()` only succeeds from `Created` state
- [ ] After successful `start()`, state is `Running`
- [ ] `ingest()` only succeeds in `Running` state
- [ ] `step()` only succeeds in `Running` or `Stopping` state
- [ ] `stop()` only succeeds from `Running` state
- [ ] After successful `stop()`, state is `Stopped`
- [ ] `stop()` clears any pending messages/queues
- [ ] A stopped kernel cannot be restarted (no backward transitions)
- [ ] Query methods (`state()`, `worker_count()`, `stats()`, etc.) work in ALL states
- [ ] `validate_config()` and `set_callbacks()` are only meaningful in `Created` state
- [ ] Destructor is safe to call in any state (calls `stop()` if still Running)

### Destructor safety

```cpp
~MyKernel() override {
    if (state_ == State::Running || state_ == State::Starting) {
        stop(5);  // Best-effort cleanup
    }
}
```

### Invalid state returns

Every state-dependent method must return `ErrorCode::State` when called in the wrong state. Never crash, never assert — return the error.

---

## 7. Callback Rules

The `KernelCallbacks` struct contains six callback slots:

```cpp
struct KernelCallbacks {
    MessageCallback on_message;               // void(string_view)
    ErrorCallback on_error;                   // void(ErrorCode, string_view)
    LogCallback on_log;                       // void(int level, string_view)
    WorkerCallback on_worker;                 // void(int id, string_view event)
    ClientConnectCallback on_client_connect;  // void(int id, string_view peer)
    ClientDisconnectCallback on_client_disconnect; // void(int id, string_view reason)
};
```

### The three rules

**Rule 1: Invoke callbacks ONLY from within `step()`.**

```cpp
// CORRECT: callback fires inside step()
int step(int timeout_ms) override {
    for (auto& msg : pending_) {
        if (callbacks_.on_message) {
            callbacks_.on_message(msg);
        }
    }
    pending_.clear();
    return events;
}

// WRONG: callback from ingest() or a background thread
Error ingest(const char* msg, size_t len) override {
    callbacks_.on_message(msg);  // ❌ NOT from step()!
    return Error::ok();
}
```

**Rule 2: Check for nullptr before invoking.**

Any callback slot may be empty. Always guard:

```cpp
if (callbacks_.on_message) {
    callbacks_.on_message(message_data);
}

if (callbacks_.on_error) {
    callbacks_.on_error(ErrorCode::Worker, "worker crashed");
}
```

**Rule 3: No reentrancy.**

Callbacks must not call back into the kernel. The user's callback code must not call `ingest()`, `stop()`, or any other kernel method from within a callback. Your kernel does not need to guard against this — it's a contract violation by the caller. But be aware: if you see strange behavior during testing, check for reentrant calls.

### Threading model

All of this happens on a single thread:

```
User thread:
  bus.step()
    → kernel_->step()
      → callbacks_.on_message(data)   ← fires here, synchronously
      → callbacks_.on_error(code, msg) ← fires here, synchronously
    ← step() returns
```

No mutexes needed inside your kernel. No background threads allowed.

---

## 8. Testing Your Kernel

### Use EchoKernel tests as a template

The `tests/test_echo_kernel.cpp` file tests every aspect of the IKernel contract. Copy its structure for your kernel:

1. **Lifecycle transitions** — verify Created → Running → Stopped
2. **State guards** — verify `ingest()` before `start()` returns `ErrorCode::State`
3. **Message round-trip** — ingest a message, call `step()`, verify callback fires
4. **Stats accuracy** — verify counters increment correctly
5. **Error conditions** — null pointer, zero length, wrong state

### Minimal test structure (GoogleTest)

```cpp
#include <gtest/gtest.h>
#include "my_kernel.hpp"

class MyKernelTest : public ::testing::Test {
protected:
    void SetUp() override {
        kernel_ = std::make_unique<myproject::MyKernel>(/* config */);
    }

    std::unique_ptr<myproject::MyKernel> kernel_;
};

TEST_F(MyKernelTest, StartsInCreatedState) {
    EXPECT_EQ(kernel_->state(), stdiobus::State::Created);
}

TEST_F(MyKernelTest, StartTransitionsToRunning) {
    stdiobus::KernelCallbacks cbs;
    kernel_->set_callbacks(cbs);
    auto err = kernel_->start();
    EXPECT_FALSE(err);
    EXPECT_EQ(kernel_->state(), stdiobus::State::Running);
}

TEST_F(MyKernelTest, IngestBeforeStartReturnsStateError) {
    auto err = kernel_->ingest("hello", 5);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), stdiobus::ErrorCode::State);
}

TEST_F(MyKernelTest, MessageDeliveredViaCallback) {
    stdiobus::KernelCallbacks cbs;
    std::string received;
    cbs.on_message = [&](std::string_view msg) { received = std::string(msg); };
    kernel_->set_callbacks(cbs);
    kernel_->start();

    kernel_->ingest(R"({"jsonrpc":"2.0","method":"test","id":1})", 42);
    kernel_->step(0);

    EXPECT_FALSE(received.empty());
}

TEST_F(MyKernelTest, StopTransitionsToStopped) {
    stdiobus::KernelCallbacks cbs;
    kernel_->set_callbacks(cbs);
    kernel_->start();
    auto err = kernel_->stop(5);
    EXPECT_FALSE(err);
    EXPECT_EQ(kernel_->state(), stdiobus::State::Stopped);
}
```

### Property-based testing

The `tests/test_kernel_properties.cpp` file verifies universal invariants that hold for ANY conformant kernel:

- For any message ingested, `stats.messages_in` increments by 1
- For any sequence of N ingests followed by `step()`, `pending_count()` goes from N to 0
- `bytes_in` equals the sum of all ingested message lengths
- State transitions never go backward

Consider running these property tests against your kernel to verify contract compliance.

### Integration testing through Bus

Once your kernel passes unit tests, test it through the full facade:

```cpp
TEST(MyKernelIntegration, WorksThroughBusBuilder) {
    std::string received;
    auto bus = stdiobus::BusBuilder()
        .kernel(std::make_unique<myproject::MyKernel>(/* config */))
        .on_message([&](std::string_view msg) { received = std::string(msg); })
        .build();

    ASSERT_TRUE(bus);
    auto err = bus.start();
    ASSERT_FALSE(err);

    bus.send(R"({"jsonrpc":"2.0","method":"ping","id":1})");
    bus.step(std::chrono::milliseconds(100));

    EXPECT_FALSE(received.empty());
    bus.stop();
}
```

---

## 9. Common Pitfalls

### Pitfall 1: Forgetting state guards

Every state-dependent method needs a guard. If you forget one, the facade may call your method in an unexpected state and get undefined behavior instead of a clean error.

```cpp
// WRONG: no state check
Error ingest(const char* msg, size_t len) override {
    queue_.push(std::string(msg, len));  // crashes if called before start()
    return Error::ok();
}

// CORRECT: guard first
Error ingest(const char* msg, size_t len) override {
    if (state_ != State::Running) {
        return Error(ErrorCode::State, "ingest() requires Running state");
    }
    // ...
}
```

### Pitfall 2: Invoking callbacks outside step()

The threading contract requires all callbacks fire from within `step()`. If you deliver messages from `ingest()` or a timer thread, the facade's assumptions break.

### Pitfall 3: Not checking callback for nullptr

```cpp
// WRONG: crashes if user didn't set on_message
callbacks_.on_message(data);

// CORRECT: guard
if (callbacks_.on_message) {
    callbacks_.on_message(data);
}
```

### Pitfall 4: Reporting interface_version > KERNEL_INTERFACE_VERSION

The facade rejects kernels that report a version higher than its own. Always return `KERNEL_INTERFACE_VERSION` or lower.

### Pitfall 5: Spawning background threads

The IKernel contract is strictly single-threaded. All I/O must happen within `step()`. If you need async I/O, use non-blocking sockets and poll them in `step()`.

### Pitfall 6: Allowing reentrancy

If your `step()` fires a callback, and that callback somehow calls back into your kernel (e.g., via a captured reference), you'll get corruption. The contract forbids this, but defensive kernels can detect it:

```cpp
int step(int timeout_ms) override {
    assert(!in_step_);  // Debug-only reentrancy detection
    in_step_ = true;
    // ... process events, fire callbacks ...
    in_step_ = false;
    return events;
}
```

### Pitfall 7: Not copying message data in ingest()

The caller may free or reuse the buffer immediately after `ingest()` returns. Your kernel must copy the data if it needs to retain it:

```cpp
Error ingest(const char* message, size_t len) override {
    // CORRECT: copy into owned storage
    pending_.emplace_back(message, len);
    return Error::ok();
}
```

### Pitfall 8: validate_config() modifying state

`validate_config()` is `const`. It must not modify the kernel. It's a pure check — "would this config work?" — not "apply this config."

### Pitfall 9: Forgetting to clear pending work on stop()

When `stop()` is called, drain or discard any queued messages. After stop, `pending_count()` should return 0.

### Pitfall 10: Returning positive error codes from register_embedded_worker()

`register_embedded_worker()` returns a worker ID (>= 0) on success, or a **negative** error code on failure. Don't accidentally return a positive error code — it'll be interpreted as a valid worker ID.

---

## Further Reading

- [kernel-interface-contract.md](kernel-interface-contract.md) — formal method contracts, preconditions, postconditions
- `include/stdiobus/echo_kernel.hpp` — reference minimal implementation (header)
- `src/echo_kernel.cpp` — reference minimal implementation (source)
- `tests/test_echo_kernel.cpp` — comprehensive test suite to use as template
- `tests/test_kernel_properties.cpp` — property-based contract invariant tests
