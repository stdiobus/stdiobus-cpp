/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_bus.cpp
 * @brief Tests for Bus RAII class
 */

#include <stdiobus/bus.hpp>

#include <gtest/gtest.h>

using namespace stdiobus;

// Note: Most Bus tests require libstdio_bus.a to be linked.
// These tests focus on API surface and behavior that can be tested
// without a running bus.

TEST(Bus, ConstructFromPath) {
    // C API creates handle even with invalid config (logs error, start() will fail)
    Bus bus("nonexistent.json");
    // Bus handle is created (C API behavior) - start() will fail later
    // The handle may or may not be valid depending on C API version
    // Just verify it doesn't crash
}

TEST(Bus, ConstructFromOptions) {
    Options opts;
    opts.config_path = "nonexistent.json";

    Bus bus(std::move(opts));
    // Same as above - C API may still create handle
}

TEST(Bus, MoveConstruction) {
    Bus bus1("nonexistent.json");
    Bus bus2(std::move(bus1));

    // bus1 should be empty after move
    EXPECT_FALSE(bus1);
}

TEST(Bus, MoveAssignment) {
    Bus bus1("nonexistent.json");
    Bus bus2("nonexistent.json");

    bus2 = std::move(bus1);
    EXPECT_FALSE(bus1);
}

// =============================================================================
// CKernel-specific "invalid config" tests
// These tests verify that CKernel rejects invalid/missing config and puts Bus
// into an invalid (Stopped) state. EchoKernel accepts any config, so these
// expectations only hold when the C kernel is the default.
// =============================================================================
#ifdef STDIOBUS_HAS_C_KERNEL

TEST(Bus, InvalidBusState) {
    Bus bus("nonexistent.json");

    // Operations on invalid bus should return appropriate errors/defaults
    EXPECT_EQ(bus.state(), State::Stopped);
    EXPECT_FALSE(bus.is_running());
    EXPECT_EQ(bus.worker_count(), 0);
    EXPECT_EQ(bus.session_count(), 0);
    EXPECT_EQ(bus.pending_count(), 0);
    EXPECT_EQ(bus.client_count(), 0);
    EXPECT_EQ(bus.poll_fd(), -1);
}

TEST(Bus, StartInvalidBus) {
    Bus bus("nonexistent.json");
    auto err = bus.start();
    EXPECT_TRUE(err);  // Should fail
}

TEST(Bus, SendInvalidBus) {
    Bus bus("nonexistent.json");
    auto err = bus.send("test");
    EXPECT_TRUE(err);  // Should fail
}

TEST(Bus, StopInvalidBus) {
    Bus bus("nonexistent.json");
    auto err = bus.stop();
    EXPECT_TRUE(err);  // Should fail
}

#endif  // STDIOBUS_HAS_C_KERNEL

// =============================================================================
// EchoKernel-specific tests (when C kernel is NOT the default)
// EchoKernel accepts any JSON config, so Bus with "nonexistent.json" is valid.
// These tests verify correct behavior in pure C++ mode.
// =============================================================================
#ifndef STDIOBUS_HAS_C_KERNEL

TEST(Bus, EchoKernel_ValidBusState) {
    Bus bus("nonexistent.json");

    // EchoKernel accepts any config — Bus is valid and in Created state
    EXPECT_TRUE(static_cast<bool>(bus));
    EXPECT_EQ(bus.state(), State::Created);
    EXPECT_TRUE(bus.is_created());
    EXPECT_FALSE(bus.is_running());
    EXPECT_FALSE(bus.is_stopped());
    EXPECT_EQ(bus.worker_count(), 0);
    EXPECT_EQ(bus.session_count(), 0);
    EXPECT_EQ(bus.pending_count(), 0);
    EXPECT_EQ(bus.client_count(), 0);
    EXPECT_EQ(bus.poll_fd(), -1);
}

TEST(Bus, EchoKernel_StartSucceeds) {
    Bus bus("nonexistent.json");
    auto err = bus.start();
    EXPECT_FALSE(err);  // Should succeed
    EXPECT_EQ(bus.state(), State::Running);
    EXPECT_TRUE(bus.is_running());
}

TEST(Bus, EchoKernel_SendSucceedsAfterStart) {
    Bus bus("nonexistent.json");
    auto start_err = bus.start();
    EXPECT_FALSE(start_err);

    auto send_err = bus.send(R"({"jsonrpc":"2.0","method":"test","id":1})");
    EXPECT_FALSE(send_err);  // Should succeed
}

TEST(Bus, EchoKernel_StopSucceedsAfterStart) {
    Bus bus("nonexistent.json");
    auto start_err = bus.start();
    EXPECT_FALSE(start_err);

    auto stop_err = bus.stop();
    EXPECT_FALSE(stop_err);  // Should succeed
    EXPECT_EQ(bus.state(), State::Stopped);
    EXPECT_TRUE(bus.is_stopped());
}

TEST(Bus, EchoKernel_SendFailsBeforeStart) {
    Bus bus("nonexistent.json");
    // Bus is in Created state — send should fail (ingest requires Running)
    auto err = bus.send("test");
    EXPECT_TRUE(err);
}

TEST(Bus, EchoKernel_StopFailsBeforeStart) {
    Bus bus("nonexistent.json");
    // Bus is in Created state — stop should fail (stop requires Running)
    auto err = bus.stop();
    EXPECT_TRUE(err);
}

#endif  // !STDIOBUS_HAS_C_KERNEL

TEST(Bus, StatsInvalidBus) {
    Bus bus("nonexistent.json");
    auto stats = bus.stats();

    // Should return zeroed stats
    EXPECT_EQ(stats.messages_in, 0u);
    EXPECT_EQ(stats.messages_out, 0u);
}

TEST(Bus, CallbackSetters) {
    Bus bus("nonexistent.json");

    bool message_called = false;
    bool error_called = false;
    bool log_called = false;
    bool worker_called = false;

    bus.on_message([&](std::string_view) { message_called = true; });
    bus.on_error([&](ErrorCode, std::string_view) { error_called = true; });
    bus.on_log([&](int, std::string_view) { log_called = true; });
    bus.on_worker([&](int, std::string_view) { worker_called = true; });

    // Callbacks are set but won't be called without valid bus
    EXPECT_FALSE(message_called);
    EXPECT_FALSE(error_called);
    EXPECT_FALSE(log_called);
    EXPECT_FALSE(worker_called);
}

TEST(BusBuilder, Default) {
    auto builder = BusBuilder();
    // Should be able to chain methods
    builder.config_path("test.json").log_level(2);
}

TEST(BusBuilder, ConfigPath) {
    auto bus = BusBuilder().config_path("nonexistent.json").build();

    // C API may create handle even with invalid config
    // start() will fail - that's the real validation point
}

TEST(BusBuilder, ConfigJson) {
    auto bus = BusBuilder().config_json(R"({"pools":[]})").build();

    // May or may not be valid depending on C library behavior
}

TEST(BusBuilder, LogLevel) {
    auto bus = BusBuilder()
                   .config_path("test.json")
                   .log_level(3)  // ERROR
                   .build();
}

TEST(BusBuilder, ListenTcp) {
    auto bus = BusBuilder().config_path("test.json").listen_tcp("127.0.0.1", 8080).build();
}

TEST(BusBuilder, ListenUnix) {
    auto bus = BusBuilder().config_path("test.json").listen_unix("/tmp/bus.sock").build();
}

TEST(BusBuilder, WithCallbacks) {
    std::string received;

    auto bus = BusBuilder()
                   .config_path("test.json")
                   .on_message([&](std::string_view msg) { received = std::string(msg); })
                   .on_error([](ErrorCode, std::string_view) {})
                   .build();
}

TEST(BusBuilder, ChainAll) {
    auto bus = BusBuilder()
                   .config_path("config.json")
                   .log_level(1)
                   .listen_tcp("0.0.0.0", 9000)
                   .on_message([](auto) {})
                   .on_error([](auto, auto) {})
                   .on_log([](auto, auto) {})
                   .on_worker([](auto, auto) {})
                   .build();
}
