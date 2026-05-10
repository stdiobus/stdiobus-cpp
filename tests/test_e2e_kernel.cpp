/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_e2e_kernel.cpp
 * @brief E2E tests verifying real system launch via both typed kernel path
 *        and JSON config path through the IKernel abstraction layer.
 *
 * These tests exercise the two BusBuilder integration paths:
 * 1. Typed path: CKernel constructed directly, injected via BusBuilder::kernel()
 * 2. JSON path: kernel_factory(c_kernel_factory()) + config_json(...)
 *
 * Both paths use /bin/cat as a real echo worker to verify full lifecycle.
 */

#ifdef STDIOBUS_HAS_C_KERNEL

#include <chrono>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <stdiobus.hpp>
#include <stdiobus/c_kernel.hpp>
#include <stdiobus/detail/kernel_config.hpp>

using namespace stdiobus;

// ============================================================================
// Test Configuration
// ============================================================================

static const char* ECHO_CONFIG_JSON = R"({
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

static const char* MULTI_WORKER_CONFIG_JSON = R"({
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
// Helpers
// ============================================================================

/**
 * @brief Wait for workers to become available by polling worker_count().
 */
static bool wait_for_workers(Bus& bus, int expected, std::chrono::seconds timeout = std::chrono::seconds(5)) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (bus.worker_count() < expected && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }
    return bus.worker_count() >= expected;
}

/**
 * @brief Wait for a certain number of messages to be received.
 */
static bool wait_for_messages(Bus& bus, const std::vector<std::string>& received,
                              size_t expected, std::chrono::seconds timeout = std::chrono::seconds(5)) {
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (received.size() < expected && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }
    return received.size() >= expected;
}

// ============================================================================
// TEST: Typed Path — Real Workers
// ============================================================================

TEST(E2E_Kernel, TypedPathRealWorkers) {
    // Create CKernel directly with typed config
    detail::KernelConfig config;
    config.config_json = ECHO_CONFIG_JSON;
    config.log_level = 2;  // WARN

    auto kernel = std::make_unique<CKernel>(config);
    ASSERT_TRUE(*kernel) << "CKernel handle should be valid after construction";

    // Inject via BusBuilder::kernel(std::move(kernel))
    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .kernel(std::move(kernel))
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus) << "Bus should be valid after typed-path construction";

    // Start — spawns real /bin/cat worker
    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    // Wait for worker to be ready
    ASSERT_TRUE(wait_for_workers(bus, 1)) << "Worker did not start within timeout";

    // Send a message
    const char* msg = R"({"jsonrpc":"2.0","method":"echo","params":{"text":"typed-path"},"id":"tp-1"})";
    err = bus.send(msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    // Wait for echo response
    ASSERT_TRUE(wait_for_messages(bus, received, 1)) << "No response received within timeout";
    EXPECT_EQ(received[0], std::string(msg));

    // Stop gracefully
    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err) << "stop() failed: " << err.message();
}

// ============================================================================
// TEST: JSON Path — Real Workers
// ============================================================================

TEST(E2E_Kernel, JsonPathRealWorkers) {
    // Use BusBuilder::kernel_factory(c_kernel_factory()) + config_json(...)
    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .kernel_factory(c_kernel_factory())
                   .config_json(ECHO_CONFIG_JSON)
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus) << "Bus should be valid after JSON-path construction";

    // Start
    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    // Wait for worker
    ASSERT_TRUE(wait_for_workers(bus, 1)) << "Worker did not start within timeout";

    // Send a message
    const char* msg = R"({"jsonrpc":"2.0","method":"echo","params":{"text":"json-path"},"id":"jp-1"})";
    err = bus.send(msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    // Wait for echo response
    ASSERT_TRUE(wait_for_messages(bus, received, 1)) << "No response received within timeout";
    EXPECT_EQ(received[0], std::string(msg));

    // Stop gracefully
    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err) << "stop() failed: " << err.message();
}

// ============================================================================
// TEST: Typed Path — Multi Worker
// ============================================================================

TEST(E2E_Kernel, TypedPathMultiWorker) {
    // Create CKernel with 3-instance pool
    detail::KernelConfig config;
    config.config_json = MULTI_WORKER_CONFIG_JSON;
    config.log_level = 2;

    auto kernel = std::make_unique<CKernel>(config);
    ASSERT_TRUE(*kernel);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .kernel(std::move(kernel))
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();

    // Wait for all 3 workers
    ASSERT_TRUE(wait_for_workers(bus, 3, std::chrono::seconds(10)))
        << "Expected 3 workers, got " << bus.worker_count();

    // Send 9 messages (3 per worker if evenly distributed)
    const int NUM_MESSAGES = 9;
    for (int i = 0; i < NUM_MESSAGES; ++i) {
        std::string m = R"({"jsonrpc":"2.0","method":"test","id":"mw-)" +
                        std::to_string(i + 1) + R"("})";
        err = bus.send(m);
        ASSERT_FALSE(err) << "send() failed on message " << i;
    }

    // Wait for all responses
    ASSERT_TRUE(wait_for_messages(bus, received, NUM_MESSAGES, std::chrono::seconds(5)))
        << "Expected " << NUM_MESSAGES << " responses, got " << received.size();

    EXPECT_EQ(static_cast<int>(received.size()), NUM_MESSAGES);

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err) << "stop() failed: " << err.message();
}

// ============================================================================
// TEST: Typed Path — Stats Accuracy
// ============================================================================

TEST(E2E_Kernel, TypedPathStats) {
    detail::KernelConfig config;
    config.config_json = ECHO_CONFIG_JSON;
    config.log_level = 2;

    auto kernel = std::make_unique<CKernel>(config);
    ASSERT_TRUE(*kernel);

    std::vector<std::string> received;

    auto bus = BusBuilder()
                   .kernel(std::move(kernel))
                   .on_message([&](std::string_view msg) { received.emplace_back(msg); })
                   .build();

    ASSERT_TRUE(bus);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();

    ASSERT_TRUE(wait_for_workers(bus, 1));

    // Send several messages and track expected byte counts
    const int NUM = 5;
    size_t total_bytes_sent = 0;

    for (int i = 0; i < NUM; ++i) {
        std::string m = R"({"jsonrpc":"2.0","method":"stats-test","id":"st-)" +
                        std::to_string(i + 1) + R"("})";
        total_bytes_sent += m.size();
        err = bus.send(m);
        ASSERT_FALSE(err);
    }

    // Wait for all responses
    ASSERT_TRUE(wait_for_messages(bus, received, NUM, std::chrono::seconds(5)));

    // Verify stats reflect the messages
    auto s = bus.stats();
    EXPECT_GE(s.messages_in, static_cast<uint64_t>(NUM))
        << "messages_in should be at least " << NUM;
    EXPECT_GE(s.messages_out, static_cast<uint64_t>(NUM))
        << "messages_out should be at least " << NUM;
    EXPECT_GE(s.bytes_out, total_bytes_sent)
        << "bytes_out should be at least " << total_bytes_sent;

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);
}

// ============================================================================
// TEST: Both Paths Equivalent
// ============================================================================

TEST(E2E_Kernel, BothPathsEquivalent) {
    // Run the same operation through both paths and verify identical observable behavior.

    const char* test_msg = R"({"jsonrpc":"2.0","method":"equiv","id":"eq-1"})";

    // --- Typed path ---
    std::vector<std::string> received_typed;
    {
        detail::KernelConfig config;
        config.config_json = ECHO_CONFIG_JSON;
        config.log_level = 2;

        auto kernel = std::make_unique<CKernel>(config);
        ASSERT_TRUE(*kernel);

        auto bus = BusBuilder()
                       .kernel(std::move(kernel))
                       .on_message([&](std::string_view msg) { received_typed.emplace_back(msg); })
                       .build();

        ASSERT_TRUE(bus);

        auto err = bus.start();
        ASSERT_FALSE(err) << "Typed path start() failed: " << err.message();

        ASSERT_TRUE(wait_for_workers(bus, 1));

        err = bus.send(test_msg);
        ASSERT_FALSE(err);

        ASSERT_TRUE(wait_for_messages(bus, received_typed, 1));

        err = bus.stop(std::chrono::seconds(5));
        EXPECT_FALSE(err);
    }

    // --- JSON path ---
    std::vector<std::string> received_json;
    {
        auto bus = BusBuilder()
                       .kernel_factory(c_kernel_factory())
                       .config_json(ECHO_CONFIG_JSON)
                       .on_message([&](std::string_view msg) { received_json.emplace_back(msg); })
                       .build();

        ASSERT_TRUE(bus);

        auto err = bus.start();
        ASSERT_FALSE(err) << "JSON path start() failed: " << err.message();

        ASSERT_TRUE(wait_for_workers(bus, 1));

        err = bus.send(test_msg);
        ASSERT_FALSE(err);

        ASSERT_TRUE(wait_for_messages(bus, received_json, 1));

        err = bus.stop(std::chrono::seconds(5));
        EXPECT_FALSE(err);
    }

    // --- Verify identical observable behavior ---
    ASSERT_EQ(received_typed.size(), received_json.size())
        << "Both paths should produce same number of responses";
    ASSERT_FALSE(received_typed.empty());

    EXPECT_EQ(received_typed[0], received_json[0])
        << "Both paths should produce identical response content";

    // Both should echo back the exact message
    EXPECT_EQ(received_typed[0], std::string(test_msg));
    EXPECT_EQ(received_json[0], std::string(test_msg));
}

#endif  // STDIOBUS_HAS_C_KERNEL
