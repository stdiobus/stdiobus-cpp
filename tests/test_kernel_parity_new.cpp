/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_kernel_parity_new.cpp
 * @brief IKernel behavioral parity tests — EchoKernel vs CKernel
 *
 * Verifies that EchoKernel and CKernel produce equivalent observable behavior
 * for the subset of operations EchoKernel supports. This ensures the abstraction
 * layer provides consistent semantics regardless of the underlying implementation.
 *
 * EchoKernel tests always run. CKernel tests are guarded by STDIOBUS_HAS_C_KERNEL.
 */

#include <stdiobus/bus.hpp>
#include <stdiobus/detail/kernel_config.hpp>
#include <stdiobus/echo_kernel.hpp>
#include <stdiobus/kernel.hpp>

#ifdef STDIOBUS_HAS_C_KERNEL
#include <stdiobus/c_kernel.hpp>
#endif

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace stdiobus;

// ============================================================================
// Helpers
// ============================================================================

namespace {

/// Valid config JSON accepted by both EchoKernel and CKernel
constexpr const char* VALID_CONFIG_JSON =
    R"({"pools":[{"id":"echo","command":"node","args":["examples/echo-worker.js"],"instances":1}]})";

/// Create an EchoKernel with valid config
std::unique_ptr<IKernel> make_echo_kernel() {
    detail::KernelConfig config;
    config.config_json = VALID_CONFIG_JSON;
    return std::make_unique<EchoKernel>(config);
}

#ifdef STDIOBUS_HAS_C_KERNEL
/// Create a CKernel with valid config
std::unique_ptr<IKernel> make_c_kernel() {
    detail::KernelConfig config;
    config.config_json = VALID_CONFIG_JSON;
    return std::make_unique<CKernel>(config);
}
#endif

/// Helper to start a kernel with an optional message callback
void start_kernel(IKernel& kernel, MessageCallback on_message = nullptr) {
    KernelCallbacks callbacks;
    callbacks.on_message = std::move(on_message);
    kernel.set_callbacks(callbacks);
    auto err = kernel.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
}

}  // namespace

// ============================================================================
// Parity: Lifecycle state transitions (Created → Running → Stopped)
// ============================================================================

TEST(KernelParityNew, echo_lifecycle_created_to_running_to_stopped) {
    auto kernel = make_echo_kernel();

    EXPECT_EQ(kernel->state(), State::Created);

    start_kernel(*kernel);
    EXPECT_EQ(kernel->state(), State::Running);

    auto err = kernel->stop(0);
    EXPECT_FALSE(err);
    EXPECT_EQ(kernel->state(), State::Stopped);
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, c_kernel_lifecycle_created_to_running_to_stopped) {
    auto kernel = make_c_kernel();

    EXPECT_EQ(kernel->state(), State::Created);

    start_kernel(*kernel);
    EXPECT_EQ(kernel->state(), State::Running);

    auto err = kernel->stop(5);
    EXPECT_FALSE(err);

    // Pump step() to allow state to transition from Stopping → Stopped
    for (int i = 0; i < 50 && kernel->state() != State::Stopped; ++i) {
        kernel->step(10);
    }
    EXPECT_EQ(kernel->state(), State::Stopped);
}
#endif

// ============================================================================
// Parity: validate_config() on valid config returns ok for both
// ============================================================================

TEST(KernelParityNew, echo_validate_config_valid_returns_ok) {
    auto kernel = make_echo_kernel();

    auto err = kernel->validate_config(VALID_CONFIG_JSON);
    EXPECT_FALSE(err) << "EchoKernel validate_config failed: " << err.message();
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, c_kernel_validate_config_valid_returns_ok) {
    auto kernel = make_c_kernel();

    auto err = kernel->validate_config(VALID_CONFIG_JSON);
    EXPECT_FALSE(err) << "CKernel validate_config failed: " << err.message();
}
#endif

// ============================================================================
// Parity: ingest() + step() delivers message via callback for both
// ============================================================================

TEST(KernelParityNew, echo_ingest_step_delivers_message) {
    auto kernel = make_echo_kernel();

    std::vector<std::string> received;
    start_kernel(*kernel, [&](std::string_view msg) {
        received.emplace_back(msg);
    });

    const std::string test_msg = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
    auto err = kernel->ingest(test_msg.data(), test_msg.size());
    EXPECT_FALSE(err) << "ingest() failed: " << err.message();

    int events = kernel->step(0);
    EXPECT_GT(events, 0);
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], test_msg);
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, c_kernel_ingest_step_delivers_message) {
    auto kernel = make_c_kernel();

    std::vector<std::string> received;
    start_kernel(*kernel, [&](std::string_view msg) {
        received.emplace_back(msg);
    });

    // Give workers time to start
    for (int i = 0; i < 50; ++i) {
        kernel->step(10);
        if (kernel->worker_count() > 0) break;
    }

    const std::string test_msg = R"({"jsonrpc":"2.0","method":"test","id":"1"})";
    auto err = kernel->ingest(test_msg.data(), test_msg.size());
    EXPECT_FALSE(err) << "ingest() failed: " << err.message();

    // Pump until message is echoed back (workers need time)
    for (int i = 0; i < 100 && received.empty(); ++i) {
        kernel->step(10);
    }

    ASSERT_GE(received.size(), 1u);
    // CKernel echoes via real worker — message content may be transformed
    // but at least one message should be delivered
    EXPECT_FALSE(received[0].empty());

    (void)kernel->stop(5);
}
#endif

// ============================================================================
// Parity: Stats counters (messages_in, messages_out) match after same operations
// ============================================================================

TEST(KernelParityNew, echo_stats_match_after_ingest_step) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    const std::string msg1 = R"({"jsonrpc":"2.0","method":"a","id":"1"})";
    const std::string msg2 = R"({"jsonrpc":"2.0","method":"b","id":"2"})";

    (void)kernel->ingest(msg1.data(), msg1.size());
    (void)kernel->ingest(msg2.data(), msg2.size());

    auto stats_before_step = kernel->stats();
    EXPECT_EQ(stats_before_step.messages_in, 2u);
    EXPECT_EQ(stats_before_step.messages_out, 0u);

    kernel->step(0);

    auto stats_after_step = kernel->stats();
    EXPECT_EQ(stats_after_step.messages_in, 2u);
    EXPECT_EQ(stats_after_step.messages_out, 2u);
    EXPECT_EQ(stats_after_step.messages_in, stats_after_step.messages_out);
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, c_kernel_stats_match_after_ingest_step) {
    auto kernel = make_c_kernel();
    start_kernel(*kernel);

    // Wait for workers to be ready
    for (int i = 0; i < 50; ++i) {
        kernel->step(10);
        if (kernel->worker_count() > 0) break;
    }

    const std::string msg1 = R"({"jsonrpc":"2.0","method":"a","id":"1"})";
    const std::string msg2 = R"({"jsonrpc":"2.0","method":"b","id":"2"})";

    (void)kernel->ingest(msg1.data(), msg1.size());
    (void)kernel->ingest(msg2.data(), msg2.size());

    // Pump until messages are processed
    for (int i = 0; i < 100; ++i) {
        kernel->step(10);
    }

    auto stats = kernel->stats();
    EXPECT_EQ(stats.messages_in, 2u);
    // CKernel with echo worker should also deliver 2 messages back
    EXPECT_EQ(stats.messages_out, stats.messages_in);

    (void)kernel->stop(5);
}
#endif

// ============================================================================
// Parity: interface_version() returns same value
// ============================================================================

TEST(KernelParityNew, echo_interface_version_matches_constant) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->interface_version(), KERNEL_INTERFACE_VERSION);
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, c_kernel_interface_version_matches_constant) {
    auto kernel = make_c_kernel();
    EXPECT_EQ(kernel->interface_version(), KERNEL_INTERFACE_VERSION);
}

TEST(KernelParityNew, both_kernels_report_same_interface_version) {
    auto echo = make_echo_kernel();
    auto c_kernel = make_c_kernel();
    EXPECT_EQ(echo->interface_version(), c_kernel->interface_version());
}
#endif

// ============================================================================
// Parity: ingest() before start() returns ErrorCode::State for both
// ============================================================================

TEST(KernelParityNew, echo_ingest_before_start_returns_state_error) {
    auto kernel = make_echo_kernel();

    ASSERT_EQ(kernel->state(), State::Created);

    auto err = kernel->ingest("hello", 5);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::State);
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, c_kernel_ingest_before_start_returns_state_error) {
    auto kernel = make_c_kernel();

    ASSERT_EQ(kernel->state(), State::Created);

    auto err = kernel->ingest("hello", 5);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::State);
}
#endif


// ============================================================================
// Parity via Typed Path: BusBuilder::kernel() for both EchoKernel and CKernel
// Validates Req 5.1, 5.3, 5.6 — typed path is tested and recommended
// ============================================================================

TEST(KernelParityNew, typed_path_echo_lifecycle_via_busbuilder) {
    detail::KernelConfig config;
    config.config_json = VALID_CONFIG_JSON;
    auto kernel = std::make_unique<EchoKernel>(config);

    std::vector<std::string> received;

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    const std::string test_msg = R"({"jsonrpc":"2.0","method":"parity","id":"tp-1"})";
    err = bus.send(test_msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    bus.step(std::chrono::milliseconds(0));

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], test_msg);

    err = bus.stop();
    EXPECT_FALSE(err);
    EXPECT_EQ(bus.state(), State::Stopped);
}

#ifdef STDIOBUS_HAS_C_KERNEL
TEST(KernelParityNew, typed_path_c_kernel_lifecycle_via_busbuilder) {
    detail::KernelConfig config;
    config.config_json = VALID_CONFIG_JSON;
    config.log_level = 2;
    auto kernel = std::make_unique<CKernel>(config);

    std::vector<std::string> received;

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);

    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    // Wait for workers
    for (int i = 0; i < 50; ++i) {
        bus.step(std::chrono::milliseconds(10));
        if (bus.worker_count() > 0) break;
    }

    const std::string test_msg = R"({"jsonrpc":"2.0","method":"parity","id":"tp-2"})";
    err = bus.send(test_msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    // Pump until message is echoed back
    for (int i = 0; i < 100 && received.empty(); ++i) {
        bus.step(std::chrono::milliseconds(10));
    }

    ASSERT_GE(received.size(), 1u);
    EXPECT_FALSE(received[0].empty());

    err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(err);

    // Pump step() to allow state to transition from Stopping → Stopped
    for (int i = 0; i < 50 && bus.state() != State::Stopped; ++i) {
        bus.step(std::chrono::milliseconds(10));
    }
    EXPECT_EQ(bus.state(), State::Stopped);
}

TEST(KernelParityNew, typed_path_both_kernels_produce_valid_bus) {
    // EchoKernel via typed path
    {
        detail::KernelConfig config;
        config.config_json = VALID_CONFIG_JSON;
        auto kernel = std::make_unique<EchoKernel>(config);

        auto bus = BusBuilder()
            .kernel(std::move(kernel))
            .build();

        EXPECT_TRUE(static_cast<bool>(bus));
        EXPECT_EQ(bus.state(), State::Created);
    }

    // CKernel via typed path
    {
        detail::KernelConfig config;
        config.config_json = VALID_CONFIG_JSON;
        config.log_level = 2;
        auto kernel = std::make_unique<CKernel>(config);

        auto bus = BusBuilder()
            .kernel(std::move(kernel))
            .build();

        EXPECT_TRUE(static_cast<bool>(bus));
        EXPECT_EQ(bus.state(), State::Created);
    }
}
#endif

TEST(KernelParityNew, typed_path_stats_match_after_ingest_step) {
    detail::KernelConfig config;
    config.config_json = VALID_CONFIG_JSON;
    auto kernel = std::make_unique<EchoKernel>(config);

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([](std::string_view) {})
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    const std::string msg1 = R"({"jsonrpc":"2.0","method":"a","id":"1"})";
    const std::string msg2 = R"({"jsonrpc":"2.0","method":"b","id":"2"})";

    ASSERT_FALSE(bus.send(msg1));
    ASSERT_FALSE(bus.send(msg2));

    auto stats_before = bus.stats();
    EXPECT_EQ(stats_before.messages_in, 2u);
    EXPECT_EQ(stats_before.messages_out, 0u);

    bus.step(std::chrono::milliseconds(0));

    auto stats_after = bus.stats();
    EXPECT_EQ(stats_after.messages_in, 2u);
    EXPECT_EQ(stats_after.messages_out, 2u);
    EXPECT_EQ(stats_after.messages_in, stats_after.messages_out);

    ASSERT_FALSE(bus.stop());
}
