/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file user_app.cpp
 * @brief Simulates a real C++ user of the stdio Bus SDK.
 * 
 * This program is compiled OUTSIDE the SDK source tree, against an
 * installed copy of the SDK (via find_package). It exercises the full
 * public API surface as a real user would.
 * 
 * Exit 0 = all checks passed. Exit 1 = failure.
 */

#include <stdiobus.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <cstdlib>

#define CHECK(cond, msg) do { \
    if (!(cond)) { \
        std::cerr << "FAIL: " << msg << std::endl; \
        failures++; \
    } else { \
        std::cerr << "  ok: " << msg << std::endl; \
        passes++; \
    } \
} while(0)

int main() {
    int passes = 0;
    int failures = 0;
    
    std::cerr << "=== stdio Bus C++ SDK — User E2E Test ===" << std::endl;
    std::cerr << std::endl;
    
    // ─── 1. Types and constants ───────────────────────────────────
    std::cerr << "--- Types & Constants ---" << std::endl;
    
    CHECK(static_cast<int>(stdiobus::State::Created) == 0, "State::Created == 0");
    CHECK(static_cast<int>(stdiobus::State::Running) == 2, "State::Running == 2");
    CHECK(static_cast<int>(stdiobus::State::Stopped) == 4, "State::Stopped == 4");
    CHECK(stdiobus::state_name(stdiobus::State::Running) == "Running", "state_name(Running)");
    
    CHECK(static_cast<int>(stdiobus::ErrorCode::Ok) == 0, "ErrorCode::Ok == 0");
    CHECK(static_cast<int>(stdiobus::ErrorCode::Timeout) == -20, "ErrorCode::Timeout == -20");
    CHECK(stdiobus::is_retryable(stdiobus::ErrorCode::Again), "Again is retryable");
    CHECK(!stdiobus::is_retryable(stdiobus::ErrorCode::Invalid), "Invalid is not retryable");
    
    // ─── 2. Error handling ────────────────────────────────────────
    std::cerr << std::endl << "--- Error Handling ---" << std::endl;
    
    stdiobus::Error ok_err;
    CHECK(!ok_err, "Default Error is ok");
    CHECK(ok_err.code() == stdiobus::ErrorCode::Ok, "Default code is Ok");
    
    stdiobus::Error timeout_err(stdiobus::ErrorCode::Timeout, "timed out");
    CHECK(timeout_err, "Timeout error is truthy");
    CHECK(timeout_err.message() == "timed out", "Error message preserved");
    CHECK(timeout_err.is_retryable(), "Timeout is retryable");
    
    auto from_c = stdiobus::Error::from_c(-10);
    CHECK(from_c.code() == stdiobus::ErrorCode::Config, "from_c(-10) == Config");
    
    // ─── 3. Bus creation with inline config ───────────────────────
    std::cerr << std::endl << "--- Bus Lifecycle (inline config, /bin/cat worker) ---" << std::endl;
    
    const char* config = R"({
        "pools": [{"id":"echo","command":"/bin/cat","args":[],"instances":1}],
        "limits": {"max_input_buffer":1048576,"max_output_queue":4194304}
    })";
    
    auto bus = stdiobus::BusBuilder()
        .config_json(config)
        .log_level(3)  // ERROR only
        .build();
    
    CHECK(bus, "Bus created from inline JSON config");
    CHECK(bus.state() == stdiobus::State::Created, "Initial state is Created");
    
    // ─── 4. Start ─────────────────────────────────────────────────
    auto start_err = bus.start();
    CHECK(!start_err, "Bus started successfully");
    
    // Wait for worker
    for (int i = 0; i < 20; ++i) {
        bus.step(std::chrono::milliseconds(50));
        if (bus.worker_count() >= 1) break;
    }
    
    CHECK(bus.is_running(), "Bus is running");
    CHECK(bus.worker_count() >= 1, "At least 1 worker running");
    
    // ─── 5. Send and receive ──────────────────────────────────────
    std::cerr << std::endl << "--- Send & Receive ---" << std::endl;
    
    std::vector<std::string> received;
    bus.on_message([&](std::string_view msg) {
        received.emplace_back(msg);
    });
    
    std::string request = R"({"jsonrpc":"2.0","method":"echo","id":"user-1","params":{"hello":"world"}})";
    auto send_err = bus.send(request);
    CHECK(!send_err, "Message sent successfully");
    
    // Pump until response
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (received.empty() && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }
    
    CHECK(!received.empty(), "Response received");
    if (!received.empty()) {
        CHECK(received[0] == request, "Echo worker returns exact message");
    }
    
    // ─── 6. Multiple messages ─────────────────────────────────────
    std::cerr << std::endl << "--- Multiple Messages ---" << std::endl;
    
    received.clear();
    const int N = 5;
    for (int i = 0; i < N; ++i) {
        std::string msg = R"({"jsonrpc":"2.0","method":"test","id":"multi-)" + 
                          std::to_string(i) + R"("})";
        bus.send(msg);
    }
    
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (static_cast<int>(received.size()) < N && std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
    }
    
    CHECK(static_cast<int>(received.size()) == N, "All 5 messages echoed back");
    
    // ─── 7. Stats ─────────────────────────────────────────────────
    std::cerr << std::endl << "--- Statistics ---" << std::endl;
    
    auto stats = bus.stats();
    CHECK(stats.messages_in >= 6u, "messages_in tracked (>= 6)");
    CHECK(stats.messages_out >= 6u, "messages_out tracked (>= 6)");
    CHECK(stats.bytes_in > 0u, "bytes_in > 0");
    CHECK(stats.bytes_out > 0u, "bytes_out > 0");
    
    // ─── 8. Graceful stop ─────────────────────────────────────────
    std::cerr << std::endl << "--- Graceful Shutdown ---" << std::endl;
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    CHECK(!stop_err, "Bus stopped gracefully");
    
    for (int i = 0; i < 50 && !bus.is_stopped(); ++i) {
        bus.step(std::chrono::milliseconds(100));
    }
    CHECK(bus.is_stopped(), "Bus reached Stopped state");
    
    // ─── 9. AsyncBus ──────────────────────────────────────────────
    std::cerr << std::endl << "--- AsyncBus ---" << std::endl;
    
    stdiobus::AsyncBus async_bus(stdiobus::Options{.config_json = std::string(config), .log_level = 3});
    auto async_start = async_bus.start();
    CHECK(!async_start, "AsyncBus started");
    
    for (int i = 0; i < 20; ++i) async_bus.pump(std::chrono::milliseconds(50));
    
    auto future = async_bus.request_async(
        R"({"jsonrpc":"2.0","method":"echo","id":"async-1","params":{}})",
        std::chrono::seconds(5)
    );
    
    deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (future.wait_for(std::chrono::milliseconds(0)) != std::future_status::ready &&
           std::chrono::steady_clock::now() < deadline) {
        async_bus.pump(std::chrono::milliseconds(50));
    }
    
    CHECK(future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready, "Async future resolved");
    if (future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
        auto result = future.get();
        CHECK(result, "Async result is success");
        CHECK(!result.response.empty(), "Async response not empty");
    }
    
    (void)async_bus.stop();
    
    // ─── Summary ──────────────────────────────────────────────────
    std::cerr << std::endl;
    std::cerr << "===========================================" << std::endl;
    std::cerr << "  Passed: " << passes << std::endl;
    std::cerr << "  Failed: " << failures << std::endl;
    std::cerr << "===========================================" << std::endl;
    
    return failures > 0 ? 1 : 0;
}
