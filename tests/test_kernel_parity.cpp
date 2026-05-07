/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_kernel_parity.cpp
 * @brief Kernel parity tests — calls the SAME C functions as tests/test_main.c
 *
 * This file ensures the C++ SDK links correctly against libstdio_bus.a and
 * that all kernel-level functions behave identically when called from C++.
 *
 * Each test here corresponds 1:1 to a test in tests/test_main.c.
 * When the kernel adds new tests, they MUST be mirrored here.
 *
 * This guarantees: if the kernel changes behavior, the C++ SDK build
 * will catch it immediately — same functions, same data, same assertions.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <unistd.h>

// Direct access to kernel C functions
extern "C" {
#include "config.h"
#include "framing.h"
#include "json_extract.h"
#include "log.h"
#include "stdio_bus.h"
#include "stdio_bus_embed.h"
}

// ============================================================================
// Helpers
// ============================================================================

static void write_temp_file(const std::string& path, const std::string& content) {
    std::ofstream f(path);
    f << content;
}

// ============================================================================
// Basic Sanity (mirrors test_version_constants, test_return_codes, etc.)
// ============================================================================

TEST(KernelParity, VersionConstants) {
    EXPECT_EQ(STDIO_BUS_VERSION_MAJOR, 0);
    EXPECT_EQ(STDIO_BUS_VERSION_MINOR, 1);
    EXPECT_EQ(STDIO_BUS_VERSION_PATCH, 0);
    EXPECT_STREQ(STDIO_BUS_VERSION_STRING, "0.1.0");
}

TEST(KernelParity, PlatformDetection) {
#if defined(STDIO_BUS_USE_EPOLL)
    EXPECT_STREQ(STDIO_BUS_PLATFORM_NAME, "linux");
#elif defined(STDIO_BUS_USE_KQUEUE)
    EXPECT_STREQ(STDIO_BUS_PLATFORM_NAME, "macos");
#else
    FAIL() << "No platform defined";
#endif
}

TEST(KernelParity, ReturnCodes) {
    EXPECT_EQ(STDIO_BUS_OK, 0);
    EXPECT_LT(STDIO_BUS_ERR, 0);
    EXPECT_LT(STDIO_BUS_EAGAIN, 0);
    EXPECT_LT(STDIO_BUS_EOF, 0);
    EXPECT_LT(STDIO_BUS_EFULL, 0);
    EXPECT_LT(STDIO_BUS_ENOTFOUND, 0);
    EXPECT_LT(STDIO_BUS_EINVAL, 0);
}

TEST(KernelParity, DefaultConstants) {
    EXPECT_GT(STDIO_BUS_DEFAULT_MAX_INPUT_BUFFER, 0);
    EXPECT_GT(STDIO_BUS_DEFAULT_MAX_OUTPUT_QUEUE, 0);
    EXPECT_GT(STDIO_BUS_DEFAULT_MAX_RESTARTS, 0);
    EXPECT_GT(STDIO_BUS_DEFAULT_RESTART_WINDOW_SEC, 0);
    EXPECT_GT(STDIO_BUS_DEFAULT_DRAIN_TIMEOUT_SEC, 0);
    EXPECT_GT(STDIO_BUS_MAX_SESSION_ID_LEN, 0);
    EXPECT_GT(STDIO_BUS_MAX_REQUEST_ID_LEN, 0);

    // Exact values (10 MB input, 40 MB output)
    EXPECT_EQ(STDIO_BUS_DEFAULT_MAX_INPUT_BUFFER, 10 * 1024 * 1024);
    EXPECT_EQ(STDIO_BUS_DEFAULT_MAX_OUTPUT_QUEUE, 40 * 1024 * 1024);

    // Output queue >= input buffer
    EXPECT_GE(STDIO_BUS_DEFAULT_MAX_OUTPUT_QUEUE, STDIO_BUS_DEFAULT_MAX_INPUT_BUFFER);

    // Per-connection under 256 MB
    EXPECT_LE(STDIO_BUS_DEFAULT_MAX_INPUT_BUFFER + STDIO_BUS_DEFAULT_MAX_OUTPUT_QUEUE,
              256 * 1024 * 1024);
}

// ============================================================================
// JSON Extraction (mirrors test_json_extract_* — Requirements 15.1, 15.3, 15.4)
// ============================================================================

TEST(KernelParity, JsonExtractAllFields) {
    const char* json =
        R"({"id":"req-123","sessionId":"sess-abc","pool":"my-worker","method":"test.method"})";
    stdio_bus_routing_fields fields;

    int ret = stdio_bus_json_extract_routing(json, strlen(json), &fields);
    ASSERT_EQ(ret, 0);

    ASSERT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.id_len, 7u);
    EXPECT_EQ(std::string(fields.id, fields.id_len), "req-123");

    ASSERT_NE(fields.session_id, nullptr);
    EXPECT_EQ(fields.session_id_len, 8u);
    EXPECT_EQ(std::string(fields.session_id, fields.session_id_len), "sess-abc");

    ASSERT_NE(fields.pool, nullptr);
    EXPECT_EQ(fields.pool_len, 9u);
    EXPECT_EQ(std::string(fields.pool, fields.pool_len), "my-worker");

    ASSERT_NE(fields.method, nullptr);
    EXPECT_EQ(fields.method_len, 11u);
    EXPECT_EQ(std::string(fields.method, fields.method_len), "test.method");

    EXPECT_FALSE(fields.is_response);
}

TEST(KernelParity, JsonExtractNumericId) {
    const char* json = R"({"id":42,"method":"test"})";
    stdio_bus_routing_fields fields;

    int ret = stdio_bus_json_extract_routing(json, strlen(json), &fields);
    ASSERT_EQ(ret, 0);

    ASSERT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.id_len, 2u);
    EXPECT_EQ(std::string(fields.id, fields.id_len), "42");
}

TEST(KernelParity, JsonExtractMissingFields) {
    stdio_bus_routing_fields fields;

    // Only id
    const char* json1 = R"({"id":"123"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    EXPECT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.session_id, nullptr);
    EXPECT_EQ(fields.method, nullptr);

    // Only sessionId
    const char* json2 = R"({"sessionId":"sess-1"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    EXPECT_EQ(fields.id, nullptr);
    EXPECT_NE(fields.session_id, nullptr);
    EXPECT_EQ(fields.method, nullptr);

    // Only method
    const char* json3 = R"({"method":"notify"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json3, strlen(json3), &fields), 0);
    EXPECT_EQ(fields.id, nullptr);
    EXPECT_EQ(fields.session_id, nullptr);
    EXPECT_NE(fields.method, nullptr);

    // Empty object
    const char* json4 = "{}";
    ASSERT_EQ(stdio_bus_json_extract_routing(json4, strlen(json4), &fields), 0);
    EXPECT_EQ(fields.id, nullptr);
    EXPECT_EQ(fields.session_id, nullptr);
    EXPECT_EQ(fields.method, nullptr);
}

TEST(KernelParity, JsonExtractResponseDetection) {
    stdio_bus_routing_fields fields;

    // Response with result
    const char* json1 = R"({"id":"1","result":{"data":"value"}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    EXPECT_TRUE(fields.is_response);

    // Response with error
    const char* json2 = R"({"id":"2","error":{"code":-32600,"message":"Invalid"}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    EXPECT_TRUE(fields.is_response);

    // Request (no result/error)
    const char* json3 = R"({"id":"3","method":"test","params":{}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json3, strlen(json3), &fields), 0);
    EXPECT_FALSE(fields.is_response);
}

TEST(KernelParity, JsonExtractMalformedMissingBraces) {
    stdio_bus_routing_fields fields;

    EXPECT_EQ(stdio_bus_json_extract_routing("\"id\":\"123\"}", 11, &fields), -1);
    EXPECT_EQ(stdio_bus_json_extract_routing("{\"id\":\"123\"", 11, &fields), -1);
    EXPECT_EQ(stdio_bus_json_extract_routing("\"hello\"", 7, &fields), -1);
    EXPECT_EQ(stdio_bus_json_extract_routing("[\"id\",\"123\"]", 12, &fields), -1);
}

TEST(KernelParity, JsonExtractMalformedUnterminatedString) {
    stdio_bus_routing_fields fields;

    EXPECT_EQ(stdio_bus_json_extract_routing("{\"id:\"123\"}", 11, &fields), -1);
    EXPECT_EQ(stdio_bus_json_extract_routing("{\"id\":\"123}", 11, &fields), -1);
    EXPECT_EQ(stdio_bus_json_extract_routing("{\"id\"\"123\"}", 11, &fields), -1);
}

TEST(KernelParity, JsonExtractEscapedStrings) {
    stdio_bus_routing_fields fields;

    // Escaped quote in id
    const char* json1 = R"({"id":"req\"123"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    ASSERT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.id_len, 8u);

    // Escaped backslash in sessionId
    const char* json2 = R"({"sessionId":"sess\\abc"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    ASSERT_NE(fields.session_id, nullptr);
    EXPECT_EQ(fields.session_id_len, 9u);

    // Multiple escape sequences
    const char* json3 = R"({"method":"test\n\t\r"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json3, strlen(json3), &fields), 0);
    ASSERT_NE(fields.method, nullptr);
    EXPECT_EQ(fields.method_len, 10u);

    // Unicode escape
    const char* json4 = R"({"id":"req\u0041"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json4, strlen(json4), &fields), 0);
    ASSERT_NE(fields.id, nullptr);
}

TEST(KernelParity, JsonExtractExtraFields) {
    stdio_bus_routing_fields fields;

    const char* json =
        R"({"jsonrpc":"2.0","id":"123","method":"test","params":{"foo":"bar","nested":{"a":1}},"sessionId":"sess-1"})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json, strlen(json), &fields), 0);
    ASSERT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.id_len, 3u);
    ASSERT_NE(fields.method, nullptr);
    ASSERT_NE(fields.session_id, nullptr);
}

TEST(KernelParity, JsonExtractNestedStructures) {
    stdio_bus_routing_fields fields;

    const char* json1 = R"({"id":"1","method":"call","params":{"obj":{"nested":{"deep":true}}}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    EXPECT_NE(fields.id, nullptr);
    EXPECT_NE(fields.method, nullptr);

    const char* json2 = R"({"id":"2","method":"call","params":[1,2,[3,4],{"a":5}]})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    EXPECT_NE(fields.id, nullptr);

    const char* json3 = R"({"id":"3","result":{"items":[{"id":1},{"id":2}]}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json3, strlen(json3), &fields), 0);
    EXPECT_TRUE(fields.is_response);
}

TEST(KernelParity, JsonExtractEdgeCases) {
    stdio_bus_routing_fields fields;

    // Empty string values
    const char* json1 = R"({"id":"","sessionId":"","method":""})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    EXPECT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.id_len, 0u);
    EXPECT_NE(fields.session_id, nullptr);
    EXPECT_EQ(fields.session_id_len, 0u);

    // Whitespace in JSON
    const char* json2 = R"(  {  "id"  :  "123"  ,  "method"  :  "test"  }  )";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    EXPECT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.id_len, 3u);

    // NULL input
    EXPECT_EQ(stdio_bus_json_extract_routing(nullptr, 0, &fields), -1);

    // Zero length
    EXPECT_EQ(stdio_bus_json_extract_routing("{}", 0, &fields), -1);
}

TEST(KernelParity, JsonExtractNonStringFields) {
    stdio_bus_routing_fields fields;

    // Numeric sessionId → skipped
    const char* json1 = R"({"id":"1","sessionId":12345})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    EXPECT_NE(fields.id, nullptr);
    EXPECT_EQ(fields.session_id, nullptr);

    // Null method → skipped
    const char* json2 = R"({"id":"1","method":null})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    EXPECT_EQ(fields.method, nullptr);

    // Boolean sessionId → skipped
    const char* json3 = R"({"id":"1","sessionId":true})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json3, strlen(json3), &fields), 0);
    EXPECT_EQ(fields.session_id, nullptr);
}

TEST(KernelParity, JsonExtractSessionIdFromParams) {
    stdio_bus_routing_fields fields;

    // sessionId in params → NOT extracted (only top-level)
    const char* json1 =
        R"({"jsonrpc":"2.0","method":"session/update","params":{"sessionId":"sess-123","update":{}}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json1, strlen(json1), &fields), 0);
    EXPECT_EQ(fields.session_id, nullptr);
    EXPECT_NE(fields.method, nullptr);
    EXPECT_EQ(fields.id, nullptr);

    // Top-level sessionId extracted, params sessionId ignored
    const char* json2 = R"({"sessionId":"top-level","params":{"sessionId":"in-params"}})";
    ASSERT_EQ(stdio_bus_json_extract_routing(json2, strlen(json2), &fields), 0);
    ASSERT_NE(fields.session_id, nullptr);
    EXPECT_EQ(fields.session_id_len, 9u);
    EXPECT_EQ(std::string(fields.session_id, fields.session_id_len), "top-level");
}

// ============================================================================
// Config Parsing (mirrors test_config_* — Requirements 12.2-12.5)
// ============================================================================

TEST(KernelParity, ConfigValidFull) {
    std::string path = "/tmp/stdiobus_cpp_test_config_valid.json";
    write_temp_file(path, R"({
        "pools": [
            {"id": "acp-worker", "command": "/usr/bin/node", "args": ["./worker.js", "--mode", "acp"], "instances": 4},
            {"id": "mcp-worker", "command": "/usr/bin/python", "args": ["worker.py"], "instances": 2}
        ],
        "limits": {
            "max_input_buffer": 2097152,
            "max_output_queue": 8388608,
            "max_restarts": 10,
            "restart_window_sec": 120,
            "drain_timeout_sec": 60,
            "backpressure_timeout_sec": 90
        }
    })");

    stdio_bus_config_t config;
    int ret = stdio_bus_config_load(path.c_str(), &config);
    ASSERT_EQ(ret, STDIO_BUS_OK);

    EXPECT_EQ(config.pool_count, 2);
    EXPECT_STREQ(config.pools[0].id, "acp-worker");
    EXPECT_STREQ(config.pools[0].command, "/usr/bin/node");
    EXPECT_EQ(config.pools[0].instance_count, 4);
    EXPECT_EQ(config.pools[0].arg_count, 3);
    EXPECT_STREQ(config.pools[0].args[0], "./worker.js");
    EXPECT_STREQ(config.pools[0].args[1], "--mode");
    EXPECT_STREQ(config.pools[0].args[2], "acp");

    EXPECT_STREQ(config.pools[1].id, "mcp-worker");
    EXPECT_EQ(config.pools[1].instance_count, 2);

    EXPECT_EQ(config.limits.max_input_buffer, 2097152u);
    EXPECT_EQ(config.limits.max_output_queue, 8388608u);
    EXPECT_EQ(config.limits.max_restarts, 10);
    EXPECT_EQ(config.limits.restart_window_sec, 120);
    EXPECT_EQ(config.limits.drain_timeout_sec, 60);
    EXPECT_EQ(config.limits.backpressure_timeout_sec, 90);

    stdio_bus_config_free(&config);
    unlink(path.c_str());
}

TEST(KernelParity, ConfigDefaultLimits) {
    std::string path = "/tmp/stdiobus_cpp_test_config_defaults.json";
    write_temp_file(path,
                    R"({"pools":[{"id":"worker","command":"/bin/echo","args":[],"instances":1}]})");

    stdio_bus_config_t config;
    ASSERT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_OK);

    EXPECT_EQ(config.limits.max_input_buffer, (size_t)STDIO_BUS_DEFAULT_MAX_INPUT_BUFFER);
    EXPECT_EQ(config.limits.max_output_queue, (size_t)STDIO_BUS_DEFAULT_MAX_OUTPUT_QUEUE);
    EXPECT_EQ(config.limits.max_restarts, STDIO_BUS_DEFAULT_MAX_RESTARTS);
    EXPECT_EQ(config.limits.restart_window_sec, STDIO_BUS_DEFAULT_RESTART_WINDOW_SEC);
    EXPECT_EQ(config.limits.drain_timeout_sec, STDIO_BUS_DEFAULT_DRAIN_TIMEOUT_SEC);
    EXPECT_EQ(config.limits.backpressure_timeout_sec, STDIO_BUS_DEFAULT_BACKPRESSURE_TIMEOUT_SEC);

    stdio_bus_config_free(&config);
    unlink(path.c_str());
}

TEST(KernelParity, ConfigMissingPoolId) {
    std::string path = "/tmp/stdiobus_cpp_test_no_id.json";
    write_temp_file(path, R"({"pools":[{"command":"/bin/echo","args":[],"instances":1}]})");

    stdio_bus_config_t config;
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);
    unlink(path.c_str());
}

TEST(KernelParity, ConfigMissingPoolCommand) {
    std::string path = "/tmp/stdiobus_cpp_test_no_cmd.json";
    write_temp_file(path, R"({"pools":[{"id":"worker","args":[],"instances":1}]})");

    stdio_bus_config_t config;
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);
    unlink(path.c_str());
}

TEST(KernelParity, ConfigEmptyPools) {
    std::string path = "/tmp/stdiobus_cpp_test_empty.json";
    write_temp_file(path, R"({"pools":[]})");

    stdio_bus_config_t config;
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);
    unlink(path.c_str());
}

TEST(KernelParity, ConfigInvalidJson) {
    std::string path = "/tmp/stdiobus_cpp_test_invalid.json";
    stdio_bus_config_t config;

    write_temp_file(path, R"({"pools": [)");
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);

    write_temp_file(path, "[1, 2, 3]");
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);

    write_temp_file(path, "{pools: []}");
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);

    unlink(path.c_str());
}

TEST(KernelParity, ConfigMissingFile) {
    stdio_bus_config_t config;
    EXPECT_EQ(stdio_bus_config_load("/tmp/nonexistent_config_99999.json", &config), STDIO_BUS_ERR);
}

TEST(KernelParity, ConfigZeroInstances) {
    std::string path = "/tmp/stdiobus_cpp_test_zero.json";
    write_temp_file(path,
                    R"({"pools":[{"id":"worker","command":"/bin/echo","args":[],"instances":0}]})");

    stdio_bus_config_t config;
    EXPECT_EQ(stdio_bus_config_load(path.c_str(), &config), STDIO_BUS_ERR);
    unlink(path.c_str());
}

TEST(KernelParity, ConfigFreeNull) {
    // Must not crash
    stdio_bus_config_free(nullptr);
}

// ============================================================================
// Framing Layer (mirrors test_framing_* — Requirements 1.x, 2.x)
// ============================================================================

TEST(KernelParity, FramingNdjsonExtractBasic) {
    const stdio_bus_framing_t* framing = stdio_bus_framing_get(STDIO_BUS_FRAME_NDJSON);
    ASSERT_NE(framing, nullptr);

    const char* input = "{\"id\":\"1\"}\n";
    size_t input_len = strlen(input);

    stdio_bus_frame_result_t result = framing->extract(input, input_len, 1048576, nullptr);
    EXPECT_EQ(result.status, STDIO_BUS_OK);
    EXPECT_EQ(result.msg_len, 10u);   // Without newline
    EXPECT_EQ(result.consumed, 11u);  // Including newline
    EXPECT_EQ(std::string(result.msg, result.msg_len), "{\"id\":\"1\"}");
}

TEST(KernelParity, FramingNdjsonExtractIncomplete) {
    const stdio_bus_framing_t* framing = stdio_bus_framing_get(STDIO_BUS_FRAME_NDJSON);
    ASSERT_NE(framing, nullptr);

    // No newline → incomplete
    const char* input = "{\"id\":\"1\"}";
    size_t input_len = strlen(input);

    stdio_bus_frame_result_t result = framing->extract(input, input_len, 1048576, nullptr);
    EXPECT_EQ(result.status, STDIO_BUS_EAGAIN);  // Incomplete
}

TEST(KernelParity, FramingNdjsonWrapBasic) {
    const stdio_bus_framing_t* framing = stdio_bus_framing_get(STDIO_BUS_FRAME_NDJSON);
    ASSERT_NE(framing, nullptr);

    const char* msg = "{\"id\":\"1\"}";
    char output[64];

    ssize_t written = framing->wrap(msg, strlen(msg), output, sizeof(output), nullptr);
    EXPECT_EQ(written, 11);  // msg + newline
    EXPECT_EQ(output[written - 1], '\n');
}

TEST(KernelParity, FramingTypeName) {
    EXPECT_STREQ(stdio_bus_framing_type_name(STDIO_BUS_FRAME_NDJSON), "ndjson");
    EXPECT_STREQ(stdio_bus_framing_type_name(STDIO_BUS_FRAME_LENGTH), "length");
    EXPECT_STREQ(stdio_bus_framing_type_name(STDIO_BUS_FRAME_RAW), "raw");
}

TEST(KernelParity, FramingGet) {
    EXPECT_NE(stdio_bus_framing_get(STDIO_BUS_FRAME_NDJSON), nullptr);
    EXPECT_NE(stdio_bus_framing_get(STDIO_BUS_FRAME_LENGTH), nullptr);
    EXPECT_NE(stdio_bus_framing_get(STDIO_BUS_FRAME_RAW), nullptr);
}

// ============================================================================
// Embed API Constants (mirrors test_ffi.cpp but validates against kernel)
// ============================================================================

TEST(KernelParity, EmbedApiVersion) {
    EXPECT_EQ(STDIO_BUS_EMBED_API_VERSION, 2);
}

TEST(KernelParity, EmbedErrorCodes) {
    EXPECT_EQ(STDIO_BUS_ERR_CONFIG, -10);
    EXPECT_EQ(STDIO_BUS_ERR_WORKER, -11);
    EXPECT_EQ(STDIO_BUS_ERR_ROUTING, -12);
    EXPECT_EQ(STDIO_BUS_ERR_BUFFER, -13);
    EXPECT_EQ(STDIO_BUS_ERR_INVALID, -14);
    EXPECT_EQ(STDIO_BUS_ERR_STATE, -15);
}

TEST(KernelParity, EmbedStateValues) {
    EXPECT_EQ(STDIO_BUS_STATE_CREATED, 0);
    EXPECT_EQ(STDIO_BUS_STATE_STARTING, 1);
    EXPECT_EQ(STDIO_BUS_STATE_RUNNING, 2);
    EXPECT_EQ(STDIO_BUS_STATE_STOPPING, 3);
    EXPECT_EQ(STDIO_BUS_STATE_STOPPED, 4);
}

TEST(KernelParity, EmbedListenModes) {
    EXPECT_EQ(STDIO_BUS_LISTEN_NONE, 0);
    EXPECT_EQ(STDIO_BUS_LISTEN_TCP, 1);
    EXPECT_EQ(STDIO_BUS_LISTEN_UNIX, 2);
}
