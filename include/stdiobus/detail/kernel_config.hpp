/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file detail/kernel_config.hpp
 * @brief Internal kernel configuration struct (not part of public API)
 *
 * This struct is an internal implementation detail used by kernel
 * implementations during the transition to JSON-only configuration.
 * It will be removed once all kernels accept std::string_view JSON directly.
 *
 * DO NOT include this header in user-facing code.
 */

#ifndef STDIOBUS_DETAIL_KERNEL_CONFIG_HPP
#define STDIOBUS_DETAIL_KERNEL_CONFIG_HPP

#include <stdiobus/types.hpp>

#include <string>

namespace stdiobus {
inline namespace v1 {
namespace detail {

/**
 * @brief Internal configuration passed to kernel on creation (transitional)
 *
 * This struct is an internal detail used by existing kernel implementations.
 * New code should use std::string_view JSON config via KernelFactory.
 */
struct KernelConfig {
    /// Path to JSON config file (one of config_path or config_json required)
    std::string config_path;

    /// Inline JSON config string (alternative to config_path)
    std::string config_json;

    /// Listener configuration
    ListenerConfig listener;

    /// Log level: 0=DEBUG, 1=INFO, 2=WARN, 3=ERROR
    int log_level = 1;
};

}  // namespace detail
}  // namespace v1
}  // namespace stdiobus

#endif  // STDIOBUS_DETAIL_KERNEL_CONFIG_HPP
