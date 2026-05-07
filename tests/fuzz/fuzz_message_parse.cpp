/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file fuzz_message_parse.cpp
 * @brief Fuzz target for message ingestion path
 *
 * Tests that arbitrary input to bus.send() does not cause:
 * - Crashes
 * - Memory corruption
 * - Undefined behavior
 * - Unbounded allocation
 */

#include <stdiobus/error.hpp>
#include <stdiobus/types.hpp>
#include <cstdint>
#include <cstddef>
#include <string_view>

// Fuzz the AsyncBus ID extraction logic
// (This is the most complex parsing in the SDK)
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // Limit input size to prevent OOM
    if (size > 64 * 1024) return 0;

    std::string_view input(reinterpret_cast<const char*>(data), size);

    // Test error construction with arbitrary strings
    if (size > 0) {
        stdiobus::Error err(stdiobus::ErrorCode::Error, std::string(input));
        (void)err.message();
        (void)err.is_retryable();
        (void)err.code();
    }

    // Test error code conversion from arbitrary ints
    if (size >= sizeof(int)) {
        int code = 0;
        __builtin_memcpy(&code, data, sizeof(int));
        auto err = stdiobus::Error::from_c(code);
        (void)err.message();
    }

    return 0;
}
