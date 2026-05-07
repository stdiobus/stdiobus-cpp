/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file stdiobus.hpp
 * @brief C++ SDK for stdio_bus — AI agent transport layer
 *
 * This is the main include file for the stdiobus C++ SDK.
 * It provides:
 * - Version information and kernel compatibility check
 * - Error handling (status-style by default, exceptions optional)
 * - Core types (State, Stats, Options, callbacks)
 * - RAII Bus class with builder pattern
 * - Async adaptor with std::future
 * - Thin FFI wrapper for direct C API access
 *
 * @par Quick Start
 * @code
 * #include <stdiobus.hpp>
 *
 * int main() {
 *     stdiobus::Bus bus("config.json");
 *
 *     bus.on_message([](std::string_view msg) {
 *         std::cout << "Received: " << msg << std::endl;
 *     });
 *
 *     if (auto err = bus.start(); err) {
 *         std::cerr << "Failed: " << err.message() << std::endl;
 *         return 1;
 *     }
 *
 *     bus.send(R"({"jsonrpc":"2.0","method":"echo","id":1})");
 *
 *     while (bus.is_running()) {
 *         bus.step(std::chrono::milliseconds(100));
 *     }
 * }
 * @endcode
 *
 * @par Thread Safety
 * Bus instances are NOT thread-safe. Use one Bus per thread or synchronize
 * externally. Callbacks are invoked from the thread calling step().
 *
 * @par ABI Compatibility
 * This SDK uses inline namespace versioning (stdiobus::v1). Binary
 * compatibility is maintained within the same major version. Linking
 * against a different major version requires recompilation.
 *
 * @see https://stdiobus.com for full documentation
 */

#ifndef STDIOBUS_HPP
#define STDIOBUS_HPP

#include <stdiobus/bus.hpp>
#include <stdiobus/error.hpp>
#include <stdiobus/export.hpp>
#include <stdiobus/types.hpp>
#include <stdiobus/version.hpp>

// Optional: thin wrapper for direct C API access
#include <stdiobus/ffi.hpp>

// Optional: async adaptor with std::future
#include <stdiobus/async.hpp>

#endif  // STDIOBUS_HPP
