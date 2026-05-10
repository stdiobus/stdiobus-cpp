/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file c_kernel.hpp
 * @brief CKernel adapter wrapping libstdio_bus.a C kernel
 *
 * CKernel is the reference IKernel implementation that delegates all
 * operations to the native C kernel library via the stdio_bus_embed.h API.
 * It provides bit-for-bit equivalent behavior to the previous direct-call
 * implementation in bus.cpp.
 *
 * This header is conditionally compiled only when STDIOBUS_HAS_C_KERNEL
 * is defined (set by CMake when libstdio_bus.a is available).
 *
 * Use cases:
 * - Production deployments with real worker processes
 * - Full-featured bus with TCP/Unix listener support
 * - Backward-compatible path for existing users
 */

#ifndef STDIOBUS_C_KERNEL_HPP
#define STDIOBUS_C_KERNEL_HPP

#ifdef STDIOBUS_HAS_C_KERNEL

#include <stdiobus/kernel.hpp>
#include <stdiobus/detail/kernel_config.hpp>

extern "C" {
#include <stdio_bus_embed.h>
}

namespace stdiobus {
inline namespace v1 {

/**
 * @brief C kernel adapter — wraps libstdio_bus.a
 *
 * Implements IKernel by delegating to the stdio_bus_* C API functions.
 * Owns the stdio_bus_t* handle and manages its lifecycle via RAII.
 *
 * Threading: All methods must be called from a single thread.
 * Callbacks are invoked synchronously from within step().
 */
class CKernel final : public IKernel {
public:
    /**
     * @brief Construct from internal kernel config
     *
     * Creates the underlying stdio_bus_t* handle via stdio_bus_create().
     */
    explicit CKernel(const detail::KernelConfig& config);

    /**
     * @brief Destructor — stops and destroys the C handle
     *
     * If in Running or Starting state, calls stdio_bus_stop() + stdio_bus_destroy().
     */
    ~CKernel() override;

    // Non-copyable
    CKernel(const CKernel&) = delete;
    CKernel& operator=(const CKernel&) = delete;

    // Movable (transfers stdio_bus_t* ownership)
    CKernel(CKernel&& other) noexcept;
    CKernel& operator=(CKernel&& other) noexcept;

    // ========== IKernel interface ==========

    int interface_version() const noexcept override { return KERNEL_INTERFACE_VERSION; }
    std::string_view name() const noexcept override { return "stdio_bus_c_kernel"; }

    [[nodiscard]] Error validate_config(std::string_view json) const override;

    void set_callbacks(const KernelCallbacks& callbacks) override;

    [[nodiscard]] Error start() override;
    int step(int timeout_ms) override;
    [[nodiscard]] Error stop(int timeout_sec) override;

    [[nodiscard]] Error ingest(const char* message, size_t len) override;

    State state() const noexcept override;
    int worker_count() const noexcept override;
    int session_count() const noexcept override;
    int pending_count() const noexcept override;
    int client_count() const noexcept override;
    int poll_fd() const noexcept override;
    Stats stats() const noexcept override;

    int register_embedded_worker(int fd_to_worker,
                                 int fd_from_worker,
                                 std::string_view pool_id) override;
    [[nodiscard]] Error unregister_embedded_worker(int worker_id) override;

    // ========== CKernel-specific ==========

    /**
     * @brief Get raw C handle (for advanced use / backward compatibility)
     * @deprecated Use IKernel interface methods instead
     */
    [[deprecated("Use IKernel interface methods instead")]]
    stdio_bus_t* raw_handle() const noexcept { return handle_; }

    /**
     * @brief Check if kernel has a valid handle
     */
    explicit operator bool() const noexcept { return handle_ != nullptr; }

private:
    stdio_bus_t* handle_ = nullptr;
    KernelCallbacks callbacks_{};
    detail::KernelConfig config_;

    // Static callback trampolines bridging C function pointers to KernelCallbacks
    static void message_trampoline(stdio_bus_t* bus, const char* msg,
                                   size_t len, void* user_data);
    static void error_trampoline(stdio_bus_t* bus, int code,
                                 const char* message, void* user_data);
    static void log_trampoline(stdio_bus_t* bus, int level,
                               const char* message, void* user_data);
    static void worker_trampoline(stdio_bus_t* bus, int worker_id,
                                  const char* event, void* user_data);
    static void client_connect_trampoline(stdio_bus_t* bus, int client_id,
                                          const char* peer_info, void* user_data);
    static void client_disconnect_trampoline(stdio_bus_t* bus, int client_id,
                                             const char* reason, void* user_data);
};

/**
 * @brief Factory function for CKernel
 *
 * Returns a KernelFactory that creates CKernel instances from JSON config.
 */
KernelFactory c_kernel_factory() noexcept;

}  // namespace v1
}  // namespace stdiobus

#endif  // STDIOBUS_HAS_C_KERNEL

#endif  // STDIOBUS_C_KERNEL_HPP
