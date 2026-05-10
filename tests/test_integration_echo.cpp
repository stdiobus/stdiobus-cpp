/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_integration_echo.cpp
 * @brief Integration tests for Bus operating through IKernel with EchoKernel
 *
 * Validates Requirement 9.3: Integration tests verifying that Bus operates
 * correctly through the IKernel interface with EchoKernel.
 * Validates Requirement 5.1, 5.3, 5.6: Typed path (BusBuilder::kernel()) is
 * tested and recommended.
 */

#include <stdiobus/bus.hpp>
#include <stdiobus/detail/kernel_config.hpp>
#include <stdiobus/echo_kernel.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace stdiobus;

// =============================================================================
// Bus construction with echo_kernel_factory() via BusBuilder
// =============================================================================

TEST(IntegrationEcho, BusBuilder_with_kernel_factory_produces_working_bus) {
    auto bus = BusBuilder()
        .config_json(R"({"test": true})")
        .kernel_factory(echo_kernel_factory())
        .build();

    EXPECT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);
}

// =============================================================================
// Full lifecycle through Bus API: build → start → send → step → receive → stop
// =============================================================================

TEST(IntegrationEcho, full_lifecycle_build_start_send_step_receive_stop) {
    std::vector<std::string> received;

    auto bus = BusBuilder()
        .config_json(R"({})")
        .kernel_factory(echo_kernel_factory())
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);

    // Start
    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    // Send
    const std::string test_msg = R"({"jsonrpc":"2.0","method":"echo","id":1})";
    err = bus.send(test_msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    // Step — delivers message back
    int events = bus.step(std::chrono::milliseconds(0));
    EXPECT_EQ(events, 1);

    // Receive
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], test_msg);

    // Stop
    err = bus.stop();
    EXPECT_FALSE(err);
    EXPECT_EQ(bus.state(), State::Stopped);
}

// =============================================================================
// BusBuilder with kernel_factory(echo_kernel_factory()) produces working Bus
// =============================================================================

TEST(IntegrationEcho, BusBuilder_kernel_factory_echo_produces_functional_bus) {
    std::string echoed;

    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .on_message([&](std::string_view msg) {
            echoed = std::string(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));

    auto err = bus.start();
    ASSERT_FALSE(err);

    err = bus.send("hello_world");
    ASSERT_FALSE(err);

    bus.step(std::chrono::milliseconds(0));
    EXPECT_EQ(echoed, "hello_world");

    err = bus.stop();
    EXPECT_FALSE(err);
}

// =============================================================================
// Bus::send() delivers message back via on_message callback after step()
// =============================================================================

TEST(IntegrationEcho, send_delivers_message_back_via_on_message_after_step) {
    std::vector<std::string> messages;

    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .on_message([&](std::string_view msg) {
            messages.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    // Send multiple messages
    ASSERT_FALSE(bus.send("msg_alpha"));
    ASSERT_FALSE(bus.send("msg_beta"));
    ASSERT_FALSE(bus.send("msg_gamma"));

    // Before step — no messages delivered yet
    EXPECT_TRUE(messages.empty());

    // Step delivers all pending
    int events = bus.step(std::chrono::milliseconds(0));
    EXPECT_EQ(events, 3);
    ASSERT_EQ(messages.size(), 3u);
    EXPECT_EQ(messages[0], "msg_alpha");
    EXPECT_EQ(messages[1], "msg_beta");
    EXPECT_EQ(messages[2], "msg_gamma");

    ASSERT_FALSE(bus.stop());
}

// =============================================================================
// Bus::stats() reflects correct counts through EchoKernel
// =============================================================================

TEST(IntegrationEcho, stats_reflect_correct_counts) {
    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .on_message([](std::string_view) {})
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    // Initial stats — all zeros
    auto s0 = bus.stats();
    EXPECT_EQ(s0.messages_in, 0u);
    EXPECT_EQ(s0.messages_out, 0u);
    EXPECT_EQ(s0.bytes_in, 0u);
    EXPECT_EQ(s0.bytes_out, 0u);

    // Send two messages
    const std::string msg1 = "twelve_char";  // 11 bytes
    const std::string msg2 = "hi";           // 2 bytes
    ASSERT_FALSE(bus.send(msg1));
    ASSERT_FALSE(bus.send(msg2));

    // After send, before step: messages_in incremented, messages_out still 0
    auto s1 = bus.stats();
    EXPECT_EQ(s1.messages_in, 2u);
    EXPECT_EQ(s1.bytes_in, 13u);
    EXPECT_EQ(s1.messages_out, 0u);
    EXPECT_EQ(s1.bytes_out, 0u);

    // After step: messages_out matches messages_in
    bus.step(std::chrono::milliseconds(0));
    auto s2 = bus.stats();
    EXPECT_EQ(s2.messages_in, 2u);
    EXPECT_EQ(s2.messages_out, 2u);
    EXPECT_EQ(s2.bytes_in, 13u);
    EXPECT_EQ(s2.bytes_out, 13u);

    ASSERT_FALSE(bus.stop());
}

// =============================================================================
// Bus::state() transitions match expected lifecycle
// =============================================================================

TEST(IntegrationEcho, state_transitions_match_lifecycle) {
    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));

    // Created
    EXPECT_EQ(bus.state(), State::Created);
    EXPECT_TRUE(bus.is_created());
    EXPECT_FALSE(bus.is_running());
    EXPECT_FALSE(bus.is_stopped());

    // Running
    ASSERT_FALSE(bus.start());
    EXPECT_EQ(bus.state(), State::Running);
    EXPECT_FALSE(bus.is_created());
    EXPECT_TRUE(bus.is_running());
    EXPECT_FALSE(bus.is_stopped());

    // Stopped
    ASSERT_FALSE(bus.stop());
    EXPECT_EQ(bus.state(), State::Stopped);
    EXPECT_FALSE(bus.is_created());
    EXPECT_FALSE(bus.is_running());
    EXPECT_TRUE(bus.is_stopped());
}

// =============================================================================
// Bus with no factory defaults correctly (EchoKernel when C kernel disabled)
// =============================================================================

#ifndef STDIOBUS_HAS_C_KERNEL
TEST(IntegrationEcho, default_factory_uses_echo_kernel_when_c_kernel_disabled) {
    std::string echoed;

    auto bus = BusBuilder()
        .on_message([&](std::string_view msg) {
            echoed = std::string(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));

    ASSERT_FALSE(bus.start());
    ASSERT_FALSE(bus.send("default_echo"));
    bus.step(std::chrono::milliseconds(0));

    EXPECT_EQ(echoed, "default_echo");
    ASSERT_FALSE(bus.stop());
}
#endif

// =============================================================================
// raw_handle() returns nullptr when backed by EchoKernel
// =============================================================================

TEST(IntegrationEcho, raw_handle_returns_nullptr_for_echo_kernel) {
    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(bus.raw_handle(), nullptr);
#pragma GCC diagnostic pop
}

// =============================================================================
// operator bool() returns true for valid EchoKernel-backed Bus
// =============================================================================

TEST(IntegrationEcho, operator_bool_returns_true_for_valid_bus) {
    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .build();

    EXPECT_TRUE(static_cast<bool>(bus));
}

TEST(IntegrationEcho, operator_bool_returns_true_after_start) {
    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .build();

    ASSERT_FALSE(bus.start());
    EXPECT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.stop());
}

// =============================================================================
// Multiple send/receive cycles in sequence
// =============================================================================

TEST(IntegrationEcho, multiple_send_receive_cycles) {
    std::vector<std::string> received;

    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    // Cycle 1
    ASSERT_FALSE(bus.send("cycle_1_msg"));
    bus.step(std::chrono::milliseconds(0));
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], "cycle_1_msg");

    // Cycle 2
    ASSERT_FALSE(bus.send("cycle_2_msg_a"));
    ASSERT_FALSE(bus.send("cycle_2_msg_b"));
    bus.step(std::chrono::milliseconds(0));
    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[1], "cycle_2_msg_a");
    EXPECT_EQ(received[2], "cycle_2_msg_b");

    // Cycle 3
    ASSERT_FALSE(bus.send("cycle_3_final"));
    bus.step(std::chrono::milliseconds(0));
    ASSERT_EQ(received.size(), 4u);
    EXPECT_EQ(received[3], "cycle_3_final");

    // Verify cumulative stats
    auto s = bus.stats();
    EXPECT_EQ(s.messages_in, 4u);
    EXPECT_EQ(s.messages_out, 4u);

    ASSERT_FALSE(bus.stop());
}

TEST(IntegrationEcho, send_receive_with_varying_message_sizes) {
    std::vector<std::string> received;

    auto bus = BusBuilder()
        .kernel_factory(echo_kernel_factory())
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    // Small message
    std::string small_msg = "x";
    ASSERT_FALSE(bus.send(small_msg));

    // Medium message
    std::string medium_msg(256, 'M');
    ASSERT_FALSE(bus.send(medium_msg));

    // Large message
    std::string large_msg(4096, 'L');
    ASSERT_FALSE(bus.send(large_msg));

    bus.step(std::chrono::milliseconds(0));

    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[0], small_msg);
    EXPECT_EQ(received[1], medium_msg);
    EXPECT_EQ(received[2], large_msg);

    auto s = bus.stats();
    EXPECT_EQ(s.bytes_in, 1u + 256u + 4096u);
    EXPECT_EQ(s.bytes_out, 1u + 256u + 4096u);

    ASSERT_FALSE(bus.stop());
}


// =============================================================================
// TYPED PATH TESTS — BusBuilder::kernel(std::move(kernel))
// Validates Req 5.1, 5.3, 5.6, 9.3
// =============================================================================

// =============================================================================
// TypedPath: BusBuilder with kernel() produces working bus
// =============================================================================

TEST(IntegrationEcho, TypedPath_BusBuilder_with_kernel_produces_working_bus) {
    detail::KernelConfig config;
    config.config_json = R"({"test": true})";
    auto kernel = std::make_unique<EchoKernel>(config);

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([](std::string_view) {})
        .build();

    EXPECT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);
}

// =============================================================================
// TypedPath: Full lifecycle — start → send → step → receive → stop
// =============================================================================

TEST(IntegrationEcho, TypedPath_full_lifecycle) {
    std::vector<std::string> received;

    detail::KernelConfig config;
    config.config_json = R"({})";
    auto kernel = std::make_unique<EchoKernel>(config);

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);

    // Start
    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    // Send
    const std::string test_msg = R"({"jsonrpc":"2.0","method":"echo","id":1})";
    err = bus.send(test_msg);
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    // Step — delivers message back
    int events = bus.step(std::chrono::milliseconds(0));
    EXPECT_EQ(events, 1);

    // Receive
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], test_msg);

    // Stop
    err = bus.stop();
    EXPECT_FALSE(err);
    EXPECT_EQ(bus.state(), State::Stopped);
}

// =============================================================================
// TypedPath: Multiple send/receive cycles
// =============================================================================

TEST(IntegrationEcho, TypedPath_multiple_send_receive_cycles) {
    std::vector<std::string> received;

    detail::KernelConfig config;
    config.config_json = R"({"test": true})";
    auto kernel = std::make_unique<EchoKernel>(config);

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    // Cycle 1
    ASSERT_FALSE(bus.send("cycle_1_msg"));
    bus.step(std::chrono::milliseconds(0));
    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], "cycle_1_msg");

    // Cycle 2
    ASSERT_FALSE(bus.send("cycle_2_msg_a"));
    ASSERT_FALSE(bus.send("cycle_2_msg_b"));
    bus.step(std::chrono::milliseconds(0));
    ASSERT_EQ(received.size(), 3u);
    EXPECT_EQ(received[1], "cycle_2_msg_a");
    EXPECT_EQ(received[2], "cycle_2_msg_b");

    // Cycle 3
    ASSERT_FALSE(bus.send("cycle_3_final"));
    bus.step(std::chrono::milliseconds(0));
    ASSERT_EQ(received.size(), 4u);
    EXPECT_EQ(received[3], "cycle_3_final");

    // Verify cumulative stats
    auto s = bus.stats();
    EXPECT_EQ(s.messages_in, 4u);
    EXPECT_EQ(s.messages_out, 4u);

    ASSERT_FALSE(bus.stop());
}

// =============================================================================
// TypedPath: Stats reflect correct counts
// =============================================================================

TEST(IntegrationEcho, TypedPath_stats_reflect_correct_counts) {
    detail::KernelConfig config;
    config.config_json = R"({"test": true})";
    auto kernel = std::make_unique<EchoKernel>(config);

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([](std::string_view) {})
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));
    ASSERT_FALSE(bus.start());

    // Initial stats — all zeros
    auto s0 = bus.stats();
    EXPECT_EQ(s0.messages_in, 0u);
    EXPECT_EQ(s0.messages_out, 0u);
    EXPECT_EQ(s0.bytes_in, 0u);
    EXPECT_EQ(s0.bytes_out, 0u);

    // Send two messages
    const std::string msg1 = "twelve_char";  // 11 bytes
    const std::string msg2 = "hi";           // 2 bytes
    ASSERT_FALSE(bus.send(msg1));
    ASSERT_FALSE(bus.send(msg2));

    // After send, before step: messages_in incremented, messages_out still 0
    auto s1 = bus.stats();
    EXPECT_EQ(s1.messages_in, 2u);
    EXPECT_EQ(s1.bytes_in, 13u);
    EXPECT_EQ(s1.messages_out, 0u);
    EXPECT_EQ(s1.bytes_out, 0u);

    // After step: messages_out matches messages_in
    bus.step(std::chrono::milliseconds(0));
    auto s2 = bus.stats();
    EXPECT_EQ(s2.messages_in, 2u);
    EXPECT_EQ(s2.messages_out, 2u);
    EXPECT_EQ(s2.bytes_in, 13u);
    EXPECT_EQ(s2.bytes_out, 13u);

    ASSERT_FALSE(bus.stop());
}

// =============================================================================
// TypedPath: raw_handle() returns nullptr for typed EchoKernel
// =============================================================================

TEST(IntegrationEcho, TypedPath_raw_handle_returns_nullptr) {
    detail::KernelConfig config;
    config.config_json = R"({"test": true})";
    auto kernel = std::make_unique<EchoKernel>(config);

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .build();

    ASSERT_TRUE(static_cast<bool>(bus));

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    EXPECT_EQ(bus.raw_handle(), nullptr);
#pragma GCC diagnostic pop
}

// =============================================================================
// TypedPath: No validate_config() called — kernel with invalid JSON still works
// =============================================================================

TEST(IntegrationEcho, TypedPath_no_validate_config_called) {
    // Use intentionally invalid JSON that would fail validation if called.
    // The typed path skips validate_config(), so the kernel should still work.
    detail::KernelConfig config;
    config.config_json = "THIS IS NOT VALID JSON AT ALL {{{";
    auto kernel = std::make_unique<EchoKernel>(config);

    std::vector<std::string> received;

    auto bus = BusBuilder()
        .kernel(std::move(kernel))
        .on_message([&](std::string_view msg) {
            received.emplace_back(msg);
        })
        .build();

    // Bus should be valid — typed path doesn't validate config
    ASSERT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);

    // Full lifecycle should work despite invalid config JSON
    auto err = bus.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
    EXPECT_EQ(bus.state(), State::Running);

    err = bus.send("typed_path_no_validate");
    ASSERT_FALSE(err) << "send() failed: " << err.message();

    bus.step(std::chrono::milliseconds(0));

    ASSERT_EQ(received.size(), 1u);
    EXPECT_EQ(received[0], "typed_path_no_validate");

    err = bus.stop();
    EXPECT_FALSE(err);
    EXPECT_EQ(bus.state(), State::Stopped);
}
