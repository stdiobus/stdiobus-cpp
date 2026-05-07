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
