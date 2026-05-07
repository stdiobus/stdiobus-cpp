/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file export.hpp
 * @brief Symbol visibility macros for shared library builds
 *
 * Currently stdiobus is distributed as a static library, so these macros
 * resolve to nothing. They are provided for future shared library support
 * and to document the public API boundary.
 */

#ifndef STDIOBUS_EXPORT_HPP
#define STDIOBUS_EXPORT_HPP

#if defined(STDIOBUS_SHARED_BUILD)
    #if defined(_WIN32)
        #if defined(STDIOBUS_EXPORTS)
            #define STDIOBUS_API __declspec(dllexport)
        #else
            #define STDIOBUS_API __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) || defined(__clang__)
        #define STDIOBUS_API __attribute__((visibility("default")))
    #else
        #define STDIOBUS_API
    #endif
#else
    #define STDIOBUS_API
#endif

/// Mark a function as deprecated with a message
#if defined(__has_cpp_attribute)
    #if __has_cpp_attribute(deprecated)
        #define STDIOBUS_DEPRECATED(msg) [[deprecated(msg)]]
    #else
        #define STDIOBUS_DEPRECATED(msg)
    #endif
#elif defined(__GNUC__) || defined(__clang__)
    #define STDIOBUS_DEPRECATED(msg) __attribute__((deprecated(msg)))
#else
    #define STDIOBUS_DEPRECATED(msg)
#endif

#endif // STDIOBUS_EXPORT_HPP
