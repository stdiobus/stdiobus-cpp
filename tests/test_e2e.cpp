/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_e2e.cpp
 * @brief End-to-end tests for stdio Bus C++ SDK
 *
 * These tests create a real bus with an echo worker (/bin/cat),
 * send messages, and verify responses come back correctly.
 */

#include <atomic>
#include <chrono>
#include <cstdio>
#include <fstream>
#include <gtest/gtest.h>
#include <stdiobus.hpp>
#include <thread>
#include <vector>

using namespace stdiobus;

/**
 * @brief Helper to create a temporary config file for tests
 */
class TempConfig {
public:
    explicit TempConfig(const std::string& json) {
        path_ = "/tmp/stdiobus_cpp_test_" + std::to_string(getpid()) + ".json";
        std::ofstream f(path_);
        f << json;
        f.close();
    }

    ~TempConfig() { std::remove(path_.c_str()); }

    const std::string& path() const { return path_; }

private:
    std::string path_;
};

/**
 * @brief Standard echo config using /bin/cat
 */
static const char* ECHO_CONFIG = R"({
    "pools": [
        {
            "id": "echo",
            "command": "/bin/cat",
            "args": [],
            "instances": 1
        }
    ],
    "limits": {
        "max_input_buffer": 1048576,
        "max_output_queue": 4194304
    }
})";

/**
 * @brief Multi-worker echo config
 */
static const char* MULTI_WORKER_CONFIG = R"({
    "pools": [
        {
            "id": "echo",
            "command": "/bin/cat",
            "args": [],
            "instances": 3
        }
    ],
    "limits": {
        "max_input_buffer": 1048576,
        "max_output_queue": 4194304
    }
})";

// ============================================================================
// E2E: Basic Lifecycle
// ============================================================================

TEST(E2E, StartAndStop) {
    TempConfig config(ECHO_CONFIG);
    Bus bus(config.path());

    ASSERT_TRUE(bus);
    EXPECT_EQ(bus.state(), State::Created);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);
    EXPECT_TRUE(bus.is_running());
    EXPECT_GE(bus.worker_count(), 1);

    // Let workers stabilize
    bus.step(std::chrono::milliseconds(100));

    err = bus.stop(std::chrono::seconds(5));
    ASSERT_FALSE(err) << "stop() failed: " << err.message();

    // Pump until stopped
    for (int i = 0; i < 50 && !bus.is_stopped(); ++i) {
        bus.step(std::chrono::milliseconds(100));
    }
    EXPECT_TRUE(bus.is_stopped());
}

TEST(E2E, SendAndReceive) {
    TempConfig config(ECHO_CONFIG);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // Send a JSON-RPC message
    const char* msg = R"({"jsonrpc":"2.0","method":"echo","params":{"text":"hello"},"id":"1"})";
    err = bus.send(msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    // Pump until we get a response (max 3 seconds)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    ASSERT_FALSE(received.empty()) << "No response received within timeout";

    // /bin/cat echoes back exactly what was sent
    EXPECT_EQ(received[0], std::string(msg));

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

TEST(E2E, MultipleMessages) {
    TempConfig config(ECHO_CONFIG);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // Send multiple messages
    const int NUM_MESSAGES = 10;
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        std::string msg =
            R"({"jsonrpc":"2.0","method":"test","id":")" + std::to_string(i + 1) + R"("})";
        err = bus.send(msg);
        ASSERT_FALSE(err) << "send() failed on message " << i;
    }

    // Pump until all responses received (max 5 seconds)
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (static_cast<int>(received.size()) < NUM_MESSAGES &&
           std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    EXPECT_EQ(static_cast<int>(received.size()), NUM_MESSAGES)
        << "Expected " << NUM_MESSAGES << " responses, got " << received.size();

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Statistics
// ============================================================================

TEST(E2E, StatsTracking) {
    TempConfig config(ECHO_CONFIG);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // Initial stats should be zero (or close to it)
    auto stats_before = bus.stats();

    // Send messages
    const int NUM = 5;
    for (int i = 0; i < NUM; ++i) {
        std::string msg =
            R"({"jsonrpc":"2.0","method":"test","id":")" + std::to_string(i + 1) + R"("})";
        (void)bus.send(msg);
    }

    // Wait for responses
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (static_cast<int>(received.size()) < NUM && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    auto stats_after = bus.stats();

    // Verify stats increased
    EXPECT_GE(stats_after.messages_in, static_cast<uint64_t>(NUM));
    EXPECT_GE(stats_after.messages_out, static_cast<uint64_t>(NUM));
    EXPECT_GT(stats_after.bytes_in, stats_before.bytes_in);
    EXPECT_GT(stats_after.bytes_out, stats_before.bytes_out);

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Multi-Worker
// ============================================================================

TEST(E2E, MultiWorkerPool) {
    TempConfig config(MULTI_WORKER_CONFIG);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let workers start
    for (int i = 0; i < 20; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    EXPECT_GE(bus.worker_count(), 3);

    // Send messages - they should be distributed across workers
    const int NUM = 9;  // 3 per worker
    for (int i = 0; i < NUM; ++i) {
        std::string msg =
            R"({"jsonrpc":"2.0","method":"test","id":")" + std::to_string(i + 1) + R"("})";
        err = bus.send(msg);
        ASSERT_FALSE(err);
    }

    // Wait for all responses
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (static_cast<int>(received.size()) < NUM && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    EXPECT_EQ(static_cast<int>(received.size()), NUM);

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Inline JSON Config
// ============================================================================

TEST(E2E, InlineJsonConfig) {
    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_json(ECHO_CONFIG)
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    const char* msg = R"({"jsonrpc":"2.0","method":"ping","id":"inline-1"})";
    err = bus.send(msg);
    ASSERT_FALSE(err);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    ASSERT_FALSE(received.empty());
    EXPECT_EQ(received[0], std::string(msg));

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Callbacks
// ============================================================================

TEST(E2E, ErrorCallback) {
    TempConfig config(ECHO_CONFIG);

    std::vector<std::pair<ErrorCode, std::string>> errors;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_error([&](ErrorCode code, std::string_view msg) {
                       errors.emplace_back(code, std::string(msg));
                   })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // Send invalid message (not valid JSON-RPC - missing required fields)
    // The bus should still route it (it's /bin/cat, it echoes anything)
    err = bus.send("not json at all");
    // May or may not error depending on routing behavior

    bus.step(std::chrono::milliseconds(200));

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

TEST(E2E, WorkerCallback) {
    TempConfig config(ECHO_CONFIG);

    std::vector<std::pair<int, std::string>> worker_events;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_worker([&](int worker_id, std::string_view event) {
                       worker_events.emplace_back(worker_id, std::string(event));
                   })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start - should trigger "started" event
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // We should have received at least one worker event
    EXPECT_FALSE(worker_events.empty()) << "Expected at least one worker event";

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Async Bus
// ============================================================================

TEST(E2E, AsyncRequestResponse) {
    TempConfig config(ECHO_CONFIG);

    AsyncBus bus(config.path());

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.pump(std::chrono::milliseconds(50));
    }

    // Send async request
    auto future = bus.request_async(
        R"({"jsonrpc":"2.0","method":"echo","params":{"msg":"async-test"},"id":"async-1"})",
        std::chrono::seconds(5));

    // Pump until ready
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
           std::chrono::steady_clock::now() < deadline) {
        bus.pump(std::chrono::milliseconds(50));
        bus.check_timeouts();
    }

    ASSERT_EQ(future.wait_for(std::chrono::milliseconds(0)), std::future_status::ready)
        << "Async request timed out";

    auto result = future.get();
    ASSERT_TRUE(result) << "Async request failed: " << result.error.message();
    EXPECT_FALSE(result.response.empty());

    // Response should contain our id
    EXPECT_NE(result.response.find("async-1"), std::string::npos);

    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

TEST(E2E, AsyncMultipleRequests) {
    TempConfig config(ECHO_CONFIG);

    AsyncBus bus(config.path());

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.pump(std::chrono::milliseconds(50));
    }

    // Send multiple async requests
    const int NUM = 5;
    std::vector<std::future<AsyncResult>> futures;

    for (int i = 0; i < NUM; ++i) {
        std::string msg =
            R"({"jsonrpc":"2.0","method":"test","id":"multi-)" + std::to_string(i + 1) + R"("})";
        futures.push_back(bus.request_async(std::move(msg), std::chrono::seconds(5)));
    }

    // Pump until all ready
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        bus.pump(std::chrono::milliseconds(50));
        bus.check_timeouts();

        bool all_ready = true;
        for (auto& f : futures) {
            if (f.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
                all_ready = false;
                break;
            }
        }
        if (all_ready)
            break;
    }

    // Verify all completed
    int completed = 0;
    for (auto& f : futures) {
        if (f.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
            auto result = f.get();
            EXPECT_TRUE(result) << "Request failed: " << result.error.message();
            completed++;
        }
    }

    EXPECT_EQ(completed, NUM) << "Not all async requests completed";

    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// E2E: Poll FD (for external event loop integration)
// ============================================================================

TEST(E2E, PollFdAvailable) {
    TempConfig config(ECHO_CONFIG);
    Bus bus(config.path());

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // poll_fd may or may not be available depending on embed API implementation.
    // In embedded mode, the event loop fd is internal to step().
    // Just verify the call doesn't crash and returns a valid value.
    int fd = bus.poll_fd();
    // fd >= 0 means available for external event loop integration
    // fd == -1 means not available (embedded mode manages its own loop)
    EXPECT_GE(fd, -1);

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Session Affinity
// ============================================================================

TEST(E2E, SessionAffinity) {
    TempConfig config(MULTI_WORKER_CONFIG);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let workers start
    for (int i = 0; i < 20; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // Send multiple messages with same sessionId - should all go to same worker
    const int NUM = 5;
    for (int i = 0; i < NUM; ++i) {
        std::string msg = R"({"jsonrpc":"2.0","method":"test","sessionId":"session-abc","id":")" +
                          std::to_string(i + 1) + R"("})";
        err = bus.send(msg);
        ASSERT_FALSE(err);
    }

    // Wait for responses
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (static_cast<int>(received.size()) < NUM && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    EXPECT_EQ(static_cast<int>(received.size()), NUM)
        << "All session-affinity messages should be echoed back";

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// E2E: Large Messages
// ============================================================================

TEST(E2E, LargeMessage) {
    TempConfig config(ECHO_CONFIG);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .config_path(config.path())
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err);

    // Let worker start
    for (int i = 0; i < 10; ++i) {
        bus.step(std::chrono::milliseconds(50));
    }

    // Build a large message (~64KB payload)
    std::string payload(65000, 'x');
    std::string msg = R"({"jsonrpc":"2.0","method":"large","params":{"data":")" + payload +
                      R"("},"id":"large-1"})";

    err = bus.send(msg);
    ASSERT_FALSE(err) << "send() large message failed: " << err.message();

    // Wait for response
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }

    ASSERT_FALSE(received.empty()) << "No response for large message";
    EXPECT_EQ(received[0].size(), msg.size()) << "Large message should echo back same size";

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}
