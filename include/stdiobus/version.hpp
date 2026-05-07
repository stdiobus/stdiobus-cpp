/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file version.hpp
 * @brief Version information for stdio Bus C++ SDK
 *
 * This header provides compile-time version constants and a runtime
 * version check against the linked C kernel library.
 */

#ifndef STDIOBUS_VERSION_HPP
#define STDIOBUS_VERSION_HPP

#include <string_view>

/// @defgroup version Version Information
/// @{

/// Major version (breaking API changes)
#define STDIOBUS_VERSION_MAJOR 1

/// Minor version (new features, backward compatible)
#define STDIOBUS_VERSION_MINOR 0

/// Patch version (bug fixes only)
#define STDIOBUS_VERSION_PATCH 0

/// Full version string
#define STDIOBUS_VERSION_STRING "1.0.0"

/// Numeric version for compile-time comparisons: (major * 10000 + minor * 100 + patch)
#define STDIOBUS_VERSION_NUMBER 10000

/// C kernel embed API version this SDK was built against
#define STDIOBUS_KERNEL_API_VERSION 2

/// @}

namespace stdiobus {
inline namespace v1 {

/**
 * @brief Get SDK version string at runtime
 * @return Version string (e.g., "1.0.0")
 */
constexpr std::string_view version() noexcept {
    return STDIOBUS_VERSION_STRING;
}

/**
 * @brief Get SDK major version
 */
constexpr int version_major() noexcept {
    return STDIOBUS_VERSION_MAJOR;
}

/**
 * @brief Get SDK minor version
 */
constexpr int version_minor() noexcept {
    return STDIOBUS_VERSION_MINOR;
}

/**
 * @brief Get SDK patch version
 */
constexpr int version_patch() noexcept {
    return STDIOBUS_VERSION_PATCH;
}

/**
 * @brief Check if the linked kernel API version is compatible
 * @return true if compatible, false if SDK/kernel version mismatch
 */
bool kernel_compatible() noexcept;

}  // namespace v1
}  // namespace stdiobus

#endif  // STDIOBUS_VERSION_HPP
