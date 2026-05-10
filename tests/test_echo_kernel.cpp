/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_echo_kernel.cpp
 * @brief Unit tests for EchoKernel (lifecycle, echo, stats, errors)
 */

#include <stdiobus/echo_kernel.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace stdiobus;

namespace {

/// Helper to create a default EchoKernel instance
std::unique_ptr<EchoKernel> make_echo_kernel() {
    detail::KernelConfig config;
    config.config_json = R"({"test": true})";
    return std::make_unique<EchoKernel>(config);
}

/// Helper to start a kernel (sets callbacks + start)
void start_kernel(EchoKernel& kernel, MessageCallback on_message = nullptr) {
    KernelCallbacks callbacks;
    callbacks.on_message = std::move(on_message);
    kernel.set_callbacks(callbacks);
    auto err = kernel.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
}

}  // namespace

// =============================================================================
// Lifecycle state transitions
// =============================================================================

TEST(EchoKernel, lifecycle_created_to_running_to_stopped) {
    auto kernel = make_echo_kernel();

    EXPECT_EQ(kernel->state(), State::Created);

    start_kernel(*kernel);
    EXPECT_EQ(kernel->state(), State::Running);

    auto err = kernel->stop(0);
    EXPECT_FALSE(err);
    EXPECT_EQ(kernel->state(), State::Stopped);
}

TEST(EchoKernel, start_in_non_created_state_returns_state_error) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    // Already Running — start again should fail
    auto err = kernel->start();
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::State);
}

TEST(EchoKernel, start_after_stopped_returns_state_error) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);
    (void)kernel->stop(0);

    // Stopped — start again should fail
    auto err = kernel->start();
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::State);
}

// =============================================================================
// Ingest state checks
// =============================================================================

TEST(EchoKernel, ingest_in_created_state_returns_state_error) {
    auto kernel = make_echo_kernel();

    auto err = kernel->ingest("hello", 5);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::State);
}

TEST(EchoKernel, ingest_in_stopped_state_returns_state_error) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);
    (void)kernel->stop(0);

    auto err = kernel->ingest("hello", 5);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::State);
}

// =============================================================================
// Message echo round-trip
// =============================================================================

TEST(EchoKernel, ingest_step_delivers_same_message) {
    auto kernel = make_echo_kernel();

    std::vector<std::string> received;
    start_kernel(*kernel, [&](std::string_view msg) {
        received.emplace_back(msg);
    });

    const std::string test_msg = R"({"jsonrpc":"2.0","method":"test"})";
    auto err = kernel->ingest(test_msg.data(), test_msg.size());
    EXPECT_FALSE(err);

    int delivered = kernel->step(0);
    EXPECT_EQ(delivered, 1);
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], test_msg);
}

TEST(EchoKernel, multiple_messages_delivered_in_single_step) {
    auto kernel = make_echo_kernel();

    std::vector<std::string> received;
    start_kernel(*kernel, [&](std::string_view msg) {
        received.emplace_back(msg);
    });

    const std::string msg1 = "message_one";
    const std::string msg2 = "message_two";
    const std::string msg3 = "message_three";

    (void)kernel->ingest(msg1.data(), msg1.size());
    (void)kernel->ingest(msg2.data(), msg2.size());
    (void)kernel->ingest(msg3.data(), msg3.size());

    int delivered = kernel->step(0);
    EXPECT_EQ(delivered, 3);
    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], msg1);
    EXPECT_EQ(received[1], msg2);
    EXPECT_EQ(received[2], msg3);
}

// =============================================================================
// pending_count accuracy
// =============================================================================

TEST(EchoKernel, pending_count_before_and_after_step) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    EXPECT_EQ(kernel->pending_count(), 0);

    (void)kernel->ingest("aaa", 3);
    (void)kernel->ingest("bbb", 3);
    EXPECT_EQ(kernel->pending_count(), 2);

    kernel->step(0);
    EXPECT_EQ(kernel->pending_count(), 0);
}

// =============================================================================
// Stats tracking
// =============================================================================

TEST(EchoKernel, stats_tracking_messages_and_bytes) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    auto s0 = kernel->stats();
    EXPECT_EQ(s0.messages_in, 0u);
    EXPECT_EQ(s0.messages_out, 0u);
    EXPECT_EQ(s0.bytes_in, 0u);
    EXPECT_EQ(s0.bytes_out, 0u);

    const std::string msg1 = "hello";  // 5 bytes
    const std::string msg2 = "world!!";  // 7 bytes

    (void)kernel->ingest(msg1.data(), msg1.size());
    (void)kernel->ingest(msg2.data(), msg2.size());

    auto s1 = kernel->stats();
    EXPECT_EQ(s1.messages_in, 2u);
    EXPECT_EQ(s1.bytes_in, 12u);
    EXPECT_EQ(s1.messages_out, 0u);
    EXPECT_EQ(s1.bytes_out, 0u);

    kernel->step(0);

    auto s2 = kernel->stats();
    EXPECT_EQ(s2.messages_in, 2u);
    EXPECT_EQ(s2.bytes_in, 12u);
    EXPECT_EQ(s2.messages_out, 2u);
    EXPECT_EQ(s2.bytes_out, 12u);
}

// =============================================================================
// Null/zero-length ingest
// =============================================================================

TEST(EchoKernel, ingest_null_pointer_returns_invalid) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    auto err = kernel->ingest(nullptr, 5);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::Invalid);
}

TEST(EchoKernel, ingest_zero_length_returns_invalid) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    auto err = kernel->ingest("data", 0);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::Invalid);
}

// =============================================================================
// stop() clears pending messages
// =============================================================================

TEST(EchoKernel, stop_clears_pending_messages) {
    auto kernel = make_echo_kernel();
    start_kernel(*kernel);

    (void)kernel->ingest("pending1", 8);
    (void)kernel->ingest("pending2", 8);
    EXPECT_EQ(kernel->pending_count(), 2);

    (void)kernel->stop(0);
    EXPECT_EQ(kernel->pending_count(), 0);
}

// =============================================================================
// register_embedded_worker returns ErrorCode::Invalid
// =============================================================================

TEST(EchoKernel, register_embedded_worker_returns_invalid) {
    auto kernel = make_echo_kernel();

    int result = kernel->register_embedded_worker(3, 4, "pool_a");
    EXPECT_EQ(result, static_cast<int>(ErrorCode::Invalid));
    EXPECT_LT(result, 0);
}

TEST(EchoKernel, unregister_embedded_worker_returns_invalid) {
    auto kernel = make_echo_kernel();

    auto err = kernel->unregister_embedded_worker(0);
    EXPECT_TRUE(err);
    EXPECT_EQ(err.code(), ErrorCode::Invalid);
}

// =============================================================================
// validate_config accepts any JSON (identity)
// =============================================================================

TEST(EchoKernel, validate_config_accepts_any_json) {
    auto kernel = make_echo_kernel();

    EXPECT_FALSE(kernel->validate_config("{}"));
    EXPECT_FALSE(kernel->validate_config(R"({"workers": []})"));
    EXPECT_FALSE(kernel->validate_config("null"));
    EXPECT_FALSE(kernel->validate_config(R"({"complex": {"nested": true}})"));
    EXPECT_FALSE(kernel->validate_config(""));
}

// =============================================================================
// Constant query methods
// =============================================================================

TEST(EchoKernel, worker_count_is_zero) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->worker_count(), 0);
}

TEST(EchoKernel, session_count_is_zero) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->session_count(), 0);
}

TEST(EchoKernel, client_count_is_zero) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->client_count(), 0);
}

TEST(EchoKernel, poll_fd_is_negative_one) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->poll_fd(), -1);
}

// =============================================================================
// Interface metadata
// =============================================================================

TEST(EchoKernel, interface_version_matches_constant) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->interface_version(), KERNEL_INTERFACE_VERSION);
}

TEST(EchoKernel, name_is_echo_kernel) {
    auto kernel = make_echo_kernel();
    EXPECT_EQ(kernel->name(), "echo_kernel");
}

// =============================================================================
// Factory function
// =============================================================================

TEST(EchoKernel, factory_creates_valid_kernel) {
    auto factory = echo_kernel_factory();
    auto kernel = factory(R"({"test": true})");

    ASSERT_NE(kernel, nullptr);
    EXPECT_EQ(kernel->state(), State::Created);
    EXPECT_EQ(kernel->name(), "echo_kernel");
}
