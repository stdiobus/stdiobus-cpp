/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_async.cpp
 * @brief Tests for async adaptor
 */

#include <stdiobus/async.hpp>

#include <gtest/gtest.h>

using namespace stdiobus;

TEST(AsyncResult, DefaultConstruction) {
    AsyncResult result;
    EXPECT_FALSE(result.error);
    EXPECT_TRUE(result.response.empty());
    EXPECT_TRUE(result);  // No error = success
}

TEST(AsyncResult, WithError) {
    AsyncResult result{Error(ErrorCode::Timeout), ""};
    EXPECT_TRUE(result.error);
    EXPECT_FALSE(result);  // Has error = failure
}

TEST(AsyncResult, WithResponse) {
    AsyncResult result{Error::ok(), R"({"result":"ok"})"};
    EXPECT_FALSE(result.error);
    EXPECT_TRUE(result);
    EXPECT_EQ(result.response, R"({"result":"ok"})");
}

TEST(AsyncBus, Construction) {
    // Bus creates handle even with invalid config
    AsyncBus bus("nonexistent.json");
    // Just verify no crash - start() behavior depends on kernel
}

TEST(AsyncBus, ConstructFromOptions) {
    Options opts;
    opts.config_path = "nonexistent.json";

    AsyncBus bus(std::move(opts));
    // Same - kernel creates handle regardless
}

// =============================================================================
// CKernel-specific "invalid config" tests
// These tests verify that CKernel rejects invalid/missing config. With CKernel,
// Bus("nonexistent.json") produces an invalid bus where all operations fail.
// EchoKernel accepts any config, so these expectations only hold with C kernel.
// =============================================================================
#ifdef STDIOBUS_HAS_C_KERNEL

TEST(AsyncBus, StartInvalid) {
    AsyncBus bus("nonexistent.json");
    auto err = bus.start();
    EXPECT_TRUE(err);
}

TEST(AsyncBus, StopInvalid) {
    AsyncBus bus("nonexistent.json");
    auto err = bus.stop();
    EXPECT_TRUE(err);
}

TEST(AsyncBus, PumpInvalid) {
    AsyncBus bus("nonexistent.json");
    // Should not crash
    int result = bus.pump(std::chrono::milliseconds(0));
    EXPECT_LT(result, 0);  // Error
}

TEST(AsyncBus, NotifyInvalid) {
    AsyncBus bus("nonexistent.json");
    auto err = bus.notify(R"({"jsonrpc":"2.0","method":"test"})");
    EXPECT_TRUE(err);
}

TEST(AsyncBus, RequestAsyncInvalid) {
    AsyncBus bus("nonexistent.json");

    auto future = bus.request_async(R"({"jsonrpc":"2.0","method":"test","id":1})");

    // Future should be ready immediately with error
    auto status = future.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);

    auto result = future.get();
    EXPECT_TRUE(result.error);
}

#endif  // STDIOBUS_HAS_C_KERNEL

// =============================================================================
// EchoKernel-specific tests (when C kernel is NOT the default)
// EchoKernel accepts any JSON config, so AsyncBus with "nonexistent.json" is
// valid and operations succeed after start().
// =============================================================================
#ifndef STDIOBUS_HAS_C_KERNEL

TEST(AsyncBus, EchoKernel_StartSucceeds) {
    AsyncBus bus("nonexistent.json");
    auto err = bus.start();
    EXPECT_FALSE(err);  // Should succeed with EchoKernel
}

TEST(AsyncBus, EchoKernel_PumpSucceeds) {
    AsyncBus bus("nonexistent.json");
    auto start_err = bus.start();
    EXPECT_FALSE(start_err);

    // Pump should succeed (returns 0 events when nothing pending)
    int result = bus.pump(std::chrono::milliseconds(0));
    EXPECT_GE(result, 0);
}

TEST(AsyncBus, EchoKernel_NotifySucceeds) {
    AsyncBus bus("nonexistent.json");
    auto start_err = bus.start();
    EXPECT_FALSE(start_err);

    auto err = bus.notify(R"({"jsonrpc":"2.0","method":"test"})");
    EXPECT_FALSE(err);  // Should succeed
}

TEST(AsyncBus, EchoKernel_StopSucceeds) {
    AsyncBus bus("nonexistent.json");
    auto start_err = bus.start();
    EXPECT_FALSE(start_err);

    auto stop_err = bus.stop();
    EXPECT_FALSE(stop_err);  // Should succeed
}

TEST(AsyncBus, EchoKernel_RequestAsyncEchoes) {
    AsyncBus bus("nonexistent.json");
    auto start_err = bus.start();
    EXPECT_FALSE(start_err);

    auto future = bus.request_async(R"({"jsonrpc":"2.0","method":"echo","id":1})");

    // Pump to deliver the echoed message
    bus.pump(std::chrono::milliseconds(0));

    auto status = future.wait_for(std::chrono::milliseconds(100));
    EXPECT_EQ(status, std::future_status::ready);

    auto result = future.get();
    EXPECT_FALSE(result.error);
    EXPECT_FALSE(result.response.empty());
}

#endif  // !STDIOBUS_HAS_C_KERNEL

TEST(AsyncBus, CheckTimeouts) {
    AsyncBus bus("nonexistent.json");
    // Should not crash
    bus.check_timeouts();
}

TEST(AsyncBus, BusAccess) {
    AsyncBus async_bus("nonexistent.json");

    Bus& bus = async_bus.bus();
    const Bus& const_bus = async_bus.bus();

    // Just verify access works without crash
    (void)bus;
    (void)const_bus;
}

// Integration test (requires valid config and libstdio_bus)
// Uncomment when running with real library
/*
TEST(AsyncBus, Integration) {
    AsyncBus bus("examples/config.json");

    ASSERT_FALSE(bus.start());

    auto future = bus.request_async(
        R"({"jsonrpc":"2.0","method":"echo","params":{"msg":"hello"},"id":1})",
        std::chrono::seconds(5)
    );

    // Pump until ready
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready) {
        bus.pump(std::chrono::milliseconds(10));
        bus.check_timeouts();
    }

    auto result = future.get();
    EXPECT_TRUE(result);
    EXPECT_FALSE(result.response.empty());

    bus.stop();
}
*/
