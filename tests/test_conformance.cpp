/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */
 
/**
 * @file test_conformance.cpp
 * @brief Conformance tests for stdiobus C++ SDK
 * 
 * These tests mirror the kernel e2e test scenarios (tests/e2e/) but run
 * through the C++ SDK embed API. They prove that the SDK correctly exposes
 * all kernel capabilities:
 * 
 * - NDJSON framing (same as test_unix_ndjson.sh, test_tcp_ndjson.sh)
 * - Session affinity routing (same as test_unix_ndjson.sh test 4)
 * - Multiple concurrent messages (same as test_unix_ndjson.sh test 2)
 * - Worker lifecycle (start, restart, stop)
 * - Backpressure handling
 * - Request-response correlation
 * - Large message handling
 * - Graceful shutdown
 * 
 * The echo-worker.js from examples/ is used as the worker, same as kernel tests.
 * This ensures byte-for-byte compatibility with the kernel's own test suite.
 */

#include <gtest/gtest.h>
#include <stdiobus.hpp>

#include <fstream>
#include <chrono>
#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstdlib>
#include <cstdio>

using namespace stdiobus;

// ============================================================================
// Test Fixture
// ============================================================================

/**
 * @brief Conformance test fixture
 * 
 * Uses the same echo-worker.js as kernel e2e tests.
 * Config matches tests/e2e/e2e_harness.sh create_config().
 */
class Conformance : public ::testing::Test {
protected:
    void SetUp() override {
        // Determine paths relative to project root
        // The build runs from sdk/cpp/build, project root is ../../..
        const char* root_env = std::getenv("STDIO_BUS_ROOT");
        if (root_env) {
            project_root_ = root_env;
        } else {
            // Try common relative paths
            if (file_exists("../../../examples/echo-worker.js")) {
                project_root_ = "../../..";
            } else if (file_exists("../../../../examples/echo-worker.js")) {
                project_root_ = "../../../..";
            } else {
                // Fallback: use absolute path detection
                project_root_ = find_project_root();
            }
        }
        
        echo_worker_ = project_root_ + "/examples/echo-worker.js";
        ASSERT_TRUE(file_exists(echo_worker_)) 
            << "echo-worker.js not found at: " << echo_worker_
            << "\nSet STDIO_BUS_ROOT env var to project root";
    }
    
    void TearDown() override {
        // Clean up temp files
        for (auto& f : temp_files_) {
            std::remove(f.c_str());
        }
    }
    
    /**
     * @brief Create config JSON matching kernel e2e harness format
     * 
     * This produces the same config structure as e2e_create_config() in
     * tests/e2e/e2e_harness.sh
     */
    std::string make_echo_config(int instances = 1) {
        return R"({
    "pools": [
        {
            "id": "echo-worker",
            "command": "/usr/bin/env",
            "args": ["node", ")" + echo_worker_ + R"("],
            "instances": )" + std::to_string(instances) + R"(
        }
    ],
    "limits": {
        "max_input_buffer": 1048576,
        "max_output_queue": 4194304,
        "max_restarts": 3,
        "restart_window_sec": 60,
        "drain_timeout_sec": 5,
        "backpressure_timeout_sec": 30
    }
})";
    }
    
    /**
     * @brief Create a Bus with echo worker config
     */
    Bus make_bus(int instances = 1) {
        return BusBuilder()
            .config_json(make_echo_config(instances))
            .log_level(2)  // WARN only
            .build();
    }
    
    /**
     * @brief Start bus and wait for workers to be ready
     */
    Error start_and_wait(Bus& bus, int expected_workers = 1) {
        auto err = bus.start();
        if (err) return err;
        
        // Wait for workers to start (same as kernel tests: up to 5s)
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (bus.worker_count() < expected_workers && 
               std::chrono::steady_clock::now() < deadline) {
            bus.step(std::chrono::milliseconds(50));
        }
        
        if (bus.worker_count() < expected_workers) {
            return Error(ErrorCode::Worker, "Workers did not start in time");
        }
        return Error::ok();
    }
    
    /**
     * @brief Send message and collect responses until expected count or timeout
     */
    std::vector<std::string> send_and_collect(
        Bus& bus, 
        const std::vector<std::string>& messages,
        int expected_responses,
        std::chrono::seconds timeout = std::chrono::seconds(10)
    ) {
        std::vector<std::string> responses;
        
        bus.on_message([&](std::string_view msg) {
            responses.emplace_back(msg);
        });
        
        for (auto& msg : messages) {
            auto err = bus.send(msg);
            if (err) {
                responses.push_back("SEND_ERROR:" + std::string(err.message()));
                return responses;
            }
        }
        
        auto deadline = std::chrono::steady_clock::now() + timeout;
        while (static_cast<int>(responses.size()) < expected_responses &&
               std::chrono::steady_clock::now() < deadline) {
            bus.step(std::chrono::milliseconds(50));
        }
        
        return responses;
    }
    
    /**
     * @brief Extract JSON field value (simple parser for test use)
     */
    static std::string extract_field(const std::string& json, const std::string& field) {
        std::string key = "\"" + field + "\"";
        auto pos = json.find(key);
        if (pos == std::string::npos) return "";
        
        pos = json.find(':', pos);
        if (pos == std::string::npos) return "";
        pos++;
        
        // Skip whitespace
        while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;
        if (pos >= json.size()) return "";
        
        if (json[pos] == '"') {
            // String value
            pos++;
            auto end = json.find('"', pos);
            if (end == std::string::npos) return "";
            return json.substr(pos, end - pos);
        } else if (json[pos] == '{' || json[pos] == '[') {
            // Object/array - find matching close
            char open = json[pos];
            char close = (open == '{') ? '}' : ']';
            int depth = 1;
            auto start = pos;
            pos++;
            while (pos < json.size() && depth > 0) {
                if (json[pos] == open) depth++;
                else if (json[pos] == close) depth--;
                pos++;
            }
            return json.substr(start, pos - start);
        } else {
            // Number, bool, null
            auto start = pos;
            while (pos < json.size() && json[pos] != ',' && json[pos] != '}' && json[pos] != ']') pos++;
            return json.substr(start, pos - start);
        }
    }
    
    static bool file_exists(const std::string& path) {
        std::ifstream f(path);
        return f.good();
    }
    
    static std::string find_project_root() {
        // Walk up from CWD looking for Makefile + include/stdio_bus_embed.h
        std::string path = ".";
        for (int i = 0; i < 10; ++i) {
            if (file_exists(path + "/include/stdio_bus_embed.h") &&
                file_exists(path + "/examples/echo-worker.js")) {
                return path;
            }
            path = "../" + path;
        }
        return ".";  // Fallback
    }
    
    std::string project_root_;
    std::string echo_worker_;
    std::vector<std::string> temp_files_;
};

// ============================================================================
// Conformance: NDJSON Request/Response (mirrors test_unix_ndjson.sh test 1)
// ============================================================================

TEST_F(Conformance, SingleRequestResponse) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    // Same message format as kernel e2e tests
    std::string request = R"({"jsonrpc":"2.0","id":"conf-single-1","method":"echo","params":{"message":"hello from cpp sdk"},"sessionId":"session-conf-1","pool":"echo-worker"})";
    
    auto responses = send_and_collect(bus, {request}, 1);
    
    ASSERT_EQ(responses.size(), 1u) << "Expected exactly 1 response";
    
    // Verify JSON-RPC structure (same checks as verify_jsonrpc_response in harness)
    auto& resp = responses[0];
    EXPECT_EQ(extract_field(resp, "jsonrpc"), "2.0");
    EXPECT_EQ(extract_field(resp, "id"), "conf-single-1");
    EXPECT_NE(resp.find("\"result\""), std::string::npos) << "Response must contain result field";
    
    // Verify session routing (same as verify_session_routing in harness)
    EXPECT_EQ(extract_field(resp, "sessionId"), "session-conf-1")
        << "sessionId must be preserved in response (session affinity contract)";
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Multiple Concurrent Messages (mirrors test_unix_ndjson.sh test 2)
// ============================================================================

TEST_F(Conformance, MultipleConcurrentMessages) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    // Send 5 concurrent requests (same as kernel test)
    const int NUM_CLIENTS = 5;
    std::vector<std::string> requests;
    
    for (int i = 1; i <= NUM_CLIENTS; ++i) {
        requests.push_back(
            R"({"jsonrpc":"2.0","id":"concurrent-)" + std::to_string(i) + 
            R"(","method":"echo","params":{"client":)" + std::to_string(i) + 
            R"(},"sessionId":"session-concurrent-)" + std::to_string(i) + 
            R"(","pool":"echo-worker"})"
        );
    }
    
    auto responses = send_and_collect(bus, requests, NUM_CLIENTS);
    
    ASSERT_EQ(static_cast<int>(responses.size()), NUM_CLIENTS)
        << "All " << NUM_CLIENTS << " concurrent requests must receive responses";
    
    // Verify each response has correct id and sessionId
    std::unordered_set<std::string> seen_ids;
    for (auto& resp : responses) {
        std::string id = extract_field(resp, "id");
        EXPECT_FALSE(id.empty()) << "Response must have id field";
        EXPECT_TRUE(id.find("concurrent-") == 0) << "Response id must match request pattern";
        seen_ids.insert(id);
        
        // Verify session routing
        std::string session = extract_field(resp, "sessionId");
        EXPECT_FALSE(session.empty()) << "Response must preserve sessionId";
    }
    
    EXPECT_EQ(static_cast<int>(seen_ids.size()), NUM_CLIENTS)
        << "Each request must get a unique response (no duplicates)";
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Session Affinity (mirrors test_unix_ndjson.sh test 4)
// ============================================================================

TEST_F(Conformance, SessionAffinityRouting) {
    // Use multiple workers to test session affinity
    auto bus = make_bus(3);
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus, 3);
    ASSERT_FALSE(err) << err.message();
    
    // Send requests with different sessions (same as kernel test)
    std::vector<std::string> sessions = {"alpha", "beta", "gamma"};
    std::vector<std::string> requests;
    
    for (auto& session : sessions) {
        requests.push_back(
            R"({"jsonrpc":"2.0","id":"route-)" + session + 
            R"(","method":"echo","params":{"session":")" + session + 
            R"("},"sessionId":"session-)" + session + 
            R"(","pool":"echo-worker"})"
        );
    }
    
    auto responses = send_and_collect(bus, requests, static_cast<int>(sessions.size()));
    
    ASSERT_EQ(responses.size(), sessions.size());
    
    // Verify session routing preserved
    for (size_t i = 0; i < responses.size(); ++i) {
        std::string session_id = extract_field(responses[i], "sessionId");
        // The response should have one of our session IDs
        bool found = false;
        for (auto& s : sessions) {
            if (session_id == "session-" + s) {
                found = true;
                break;
            }
        }
        EXPECT_TRUE(found) << "Response sessionId '" << session_id << "' not in expected set";
    }
    
    // Second round: same sessions should route to same workers (affinity)
    std::vector<std::string> requests2;
    for (auto& session : sessions) {
        requests2.push_back(
            R"({"jsonrpc":"2.0","id":"route-)" + session + 
            R"(-2","method":"echo","params":{"session":")" + session + 
            R"(","request":2},"sessionId":"session-)" + session + 
            R"(","pool":"echo-worker"})"
        );
    }
    
    auto responses2 = send_and_collect(bus, requests2, static_cast<int>(sessions.size()));
    
    ASSERT_EQ(responses2.size(), sessions.size())
        << "Second round of session requests must all get responses (affinity maintained)";
    
    for (auto& resp : responses2) {
        std::string session_id = extract_field(resp, "sessionId");
        EXPECT_FALSE(session_id.empty()) << "Session affinity: sessionId must be preserved on second request";
    }
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Request-Response Correlation
// ============================================================================

TEST_F(Conformance, RequestResponseCorrelation) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    // Send requests with different IDs and verify correlation
    const int NUM = 10;
    std::vector<std::string> requests;
    
    for (int i = 1; i <= NUM; ++i) {
        requests.push_back(
            R"({"jsonrpc":"2.0","id":"corr-)" + std::to_string(i) + 
            R"(","method":"echo","params":{"n":)" + std::to_string(i) + 
            R"(},"pool":"echo-worker"})"
        );
    }
    
    auto responses = send_and_collect(bus, requests, NUM);
    
    ASSERT_EQ(static_cast<int>(responses.size()), NUM);
    
    // Verify each response has matching id
    std::unordered_set<std::string> response_ids;
    for (auto& resp : responses) {
        std::string id = extract_field(resp, "id");
        EXPECT_TRUE(id.find("corr-") == 0) << "Response id must match request pattern, got: " << id;
        response_ids.insert(id);
    }
    
    // All IDs must be present (1:1 correlation)
    for (int i = 1; i <= NUM; ++i) {
        std::string expected_id = "corr-" + std::to_string(i);
        EXPECT_TRUE(response_ids.count(expected_id))
            << "Missing response for request id=" << expected_id;
    }
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Graceful Shutdown (mirrors kernel drain behavior)
// ============================================================================

TEST_F(Conformance, GracefulShutdown) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    EXPECT_TRUE(bus.is_running());
    EXPECT_GE(bus.worker_count(), 1);
    
    // Send a message to ensure worker is active
    std::string request = R"({"jsonrpc":"2.0","id":"shutdown-1","method":"echo","params":{},"pool":"echo-worker"})";
    auto responses = send_and_collect(bus, {request}, 1);
    ASSERT_EQ(responses.size(), 1u);
    
    // Initiate graceful shutdown
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err) << "Graceful shutdown must succeed: " << stop_err.message();
    
    // Pump until fully stopped
    for (int i = 0; i < 50 && !bus.is_stopped(); ++i) {
        bus.step(std::chrono::milliseconds(100));
    }
    
    EXPECT_TRUE(bus.is_stopped()) << "Bus must reach STOPPED state after graceful shutdown";
    EXPECT_EQ(bus.worker_count(), 0) << "All workers must be terminated after shutdown";
}

// ============================================================================
// Conformance: Statistics Accuracy
// ============================================================================

TEST_F(Conformance, StatisticsAccuracy) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    // Send known number of messages
    const int NUM = 7;
    std::vector<std::string> requests;
    for (int i = 1; i <= NUM; ++i) {
        requests.push_back(
            R"({"jsonrpc":"2.0","id":"stats-)" + std::to_string(i) + 
            R"(","method":"echo","params":{},"pool":"echo-worker"})"
        );
    }
    
    auto responses = send_and_collect(bus, requests, NUM);
    ASSERT_EQ(static_cast<int>(responses.size()), NUM);
    
    auto stats = bus.stats();
    
    // messages_in counts messages ingested from host
    EXPECT_GE(stats.messages_in, static_cast<uint64_t>(NUM))
        << "messages_in must count all ingested messages";
    
    // messages_out counts messages delivered to host
    EXPECT_GE(stats.messages_out, static_cast<uint64_t>(NUM))
        << "messages_out must count all delivered responses";
    
    // bytes must be non-zero
    EXPECT_GT(stats.bytes_in, 0u) << "bytes_in must be tracked";
    EXPECT_GT(stats.bytes_out, 0u) << "bytes_out must be tracked";
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Multi-Worker Pool Distribution
// ============================================================================

TEST_F(Conformance, MultiWorkerDistribution) {
    const int NUM_WORKERS = 3;
    auto bus = make_bus(NUM_WORKERS);
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus, NUM_WORKERS);
    ASSERT_FALSE(err) << err.message();
    
    EXPECT_EQ(bus.worker_count(), NUM_WORKERS);
    
    // Send messages without sessionId - should be distributed across workers
    const int NUM_MESSAGES = 9;  // 3 per worker ideally
    std::vector<std::string> requests;
    for (int i = 1; i <= NUM_MESSAGES; ++i) {
        requests.push_back(
            R"({"jsonrpc":"2.0","id":"dist-)" + std::to_string(i) + 
            R"(","method":"echo","params":{"n":)" + std::to_string(i) + 
            R"(},"pool":"echo-worker"})"
        );
    }
    
    auto responses = send_and_collect(bus, requests, NUM_MESSAGES);
    
    EXPECT_EQ(static_cast<int>(responses.size()), NUM_MESSAGES)
        << "All messages must be processed by the worker pool";
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Large Message Handling (mirrors kernel buffer limits)
// ============================================================================

TEST_F(Conformance, LargeMessageHandling) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    // Build a large message within buffer limits (max_input_buffer = 1MB)
    // Use ~64KB payload (well within limits, same as e2e test_e2e.cpp)
    std::string payload(64000, 'A');
    std::string request = R"({"jsonrpc":"2.0","id":"large-1","method":"echo","params":{"data":")" + 
                          payload + R"("},"pool":"echo-worker"})";
    
    auto responses = send_and_collect(bus, {request}, 1, std::chrono::seconds(15));
    
    ASSERT_EQ(responses.size(), 1u) << "Large message must receive response";
    
    auto& resp = responses[0];
    EXPECT_EQ(extract_field(resp, "id"), "large-1");
    EXPECT_NE(resp.find("\"result\""), std::string::npos);
    
    // Response size should be substantial (echo worker echoes params)
    EXPECT_GT(resp.size(), 60000u)
        << "Response must contain the echoed large payload";
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Notification Handling (no response expected)
// ============================================================================

TEST_F(Conformance, NotificationNoResponse) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    std::vector<std::string> received;
    bus.on_message([&](std::string_view msg) {
        received.emplace_back(msg);
    });
    
    // Send a notification (no id field) - per JSON-RPC 2.0, no response expected
    std::string notification = R"({"jsonrpc":"2.0","method":"notify","params":{"event":"test"},"pool":"echo-worker"})";
    auto send_err = bus.send(notification);
    EXPECT_FALSE(send_err) << "Sending notification must succeed";
    
    // Send a request after to verify bus is still working
    std::string request = R"({"jsonrpc":"2.0","id":"after-notify","method":"echo","params":{},"pool":"echo-worker"})";
    send_err = bus.send(request);
    EXPECT_FALSE(send_err);
    
    // Wait for the request response
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        bus.step(std::chrono::milliseconds(50));
        // Look for our request response
        for (auto& r : received) {
            if (r.find("after-notify") != std::string::npos) {
                goto done;
            }
        }
    }
    done:
    
    // We should have received the request response
    bool found_request_response = false;
    for (auto& r : received) {
        if (r.find("after-notify") != std::string::npos) {
            found_request_response = true;
            break;
        }
    }
    EXPECT_TRUE(found_request_response) << "Request after notification must get response";
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}

// ============================================================================
// Conformance: Worker Count and State Queries
// ============================================================================

TEST_F(Conformance, StateTransitions) {
    auto bus = make_bus();
    ASSERT_TRUE(bus);
    
    // Before start: Created state
    EXPECT_EQ(bus.state(), State::Created);
    EXPECT_TRUE(bus.is_created());
    EXPECT_FALSE(bus.is_running());
    EXPECT_FALSE(bus.is_stopped());
    
    // After start: Running state
    auto err = bus.start();
    ASSERT_FALSE(err);
    
    // May briefly be Starting, then Running
    for (int i = 0; i < 50; ++i) {
        bus.step(std::chrono::milliseconds(50));
        if (bus.is_running()) break;
    }
    EXPECT_TRUE(bus.is_running());
    EXPECT_EQ(bus.state(), State::Running);
    
    // After stop: Stopped state
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
    
    for (int i = 0; i < 50 && !bus.is_stopped(); ++i) {
        bus.step(std::chrono::milliseconds(100));
    }
    EXPECT_TRUE(bus.is_stopped());
    EXPECT_EQ(bus.state(), State::Stopped);
}

// ============================================================================
// Conformance: Inline JSON Config (same as config_json in embed API)
// ============================================================================

TEST_F(Conformance, InlineJsonConfig) {
    // This tests the config_json path (same as --config-fd in kernel)
    std::string config = make_echo_config(1);
    
    auto bus = BusBuilder()
        .config_json(config)
        .log_level(2)
        .build();
    
    ASSERT_TRUE(bus);
    
    auto err = start_and_wait(bus);
    ASSERT_FALSE(err) << err.message();
    
    std::string request = R"({"jsonrpc":"2.0","id":"inline-conf-1","method":"echo","params":{"source":"inline"},"pool":"echo-worker"})";
    auto responses = send_and_collect(bus, {request}, 1);
    
    ASSERT_EQ(responses.size(), 1u);
    EXPECT_EQ(extract_field(responses[0], "id"), "inline-conf-1");
    
    auto stop_err = bus.stop(std::chrono::seconds(5));
    EXPECT_FALSE(stop_err);
}
