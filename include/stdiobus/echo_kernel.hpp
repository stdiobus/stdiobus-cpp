/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file echo_kernel.hpp
 * @brief Built-in echo/loopback kernel implementation
 *
 * EchoKernel is a minimal, in-process kernel implementation that echoes
 * ingested messages back through the on_message callback. It requires no
 * external processes, no libstdio_bus.a, and no platform-specific I/O.
 *
 * Use cases:
 * - Unit testing without subprocess overhead
 * - CI environments without the C kernel binary
 * - Validating the IKernel interface contract
 * - Quick prototyping and demos
 *
 * Behavior:
 * - ingest() queues the message internally
 * - step() delivers queued messages via on_message callback
 * - No real workers are spawned (worker_count() always returns 0)
 * - State machine follows the standard lifecycle
 */

#ifndef STDIOBUS_ECHO_KERNEL_HPP
#define STDIOBUS_ECHO_KERNEL_HPP

#include <stdiobus/kernel.hpp>
#include <stdiobus/detail/kernel_config.hpp>

#include <deque>
#include <string>

namespace stdiobus {
inline namespace v1 {

/**
 * @brief In-process echo/loopback kernel
 *
 * Implements IKernel by echoing messages back to the host.
 * No external dependencies. Pure C++ implementation.
 */
class EchoKernel final : public IKernel {
public:
    explicit EchoKernel(const detail::KernelConfig& config);
    ~EchoKernel() override = default;

    // Non-copyable, movable
    EchoKernel(const EchoKernel&) = delete;
    EchoKernel& operator=(const EchoKernel&) = delete;
    EchoKernel(EchoKernel&&) noexcept = default;
    EchoKernel& operator=(EchoKernel&&) noexcept = default;

    // ========== IKernel interface ==========

    int interface_version() const noexcept override { return KERNEL_INTERFACE_VERSION; }
    std::string_view name() const noexcept override { return "echo_kernel"; }

    [[nodiscard]] Error validate_config(std::string_view json) const override;

    void set_callbacks(const KernelCallbacks& callbacks) override;

    [[nodiscard]] Error start() override;
    int step(int timeout_ms) override;
    [[nodiscard]] Error stop(int timeout_sec) override;

    [[nodiscard]] Error ingest(const char* message, size_t len) override;

    State state() const noexcept override { return state_; }
    int worker_count() const noexcept override { return 0; }
    int session_count() const noexcept override { return 0; }
    int pending_count() const noexcept override;
    int client_count() const noexcept override { return 0; }
    int poll_fd() const noexcept override { return -1; }
    Stats stats() const noexcept override { return stats_; }

    int register_embedded_worker(int fd_to_worker,
                                 int fd_from_worker,
                                 std::string_view pool_id) override;
    [[nodiscard]] Error unregister_embedded_worker(int worker_id) override;

private:
    State state_ = State::Created;
    KernelCallbacks callbacks_{};
    detail::KernelConfig config_;
    Stats stats_{};
    std::deque<std::string> pending_messages_;
};

/**
 * @brief Factory function for EchoKernel
 *
 * Returns a KernelFactory that creates EchoKernel instances.
 */
KernelFactory echo_kernel_factory() noexcept;

}  // namespace v1
}  // namespace stdiobus

#endif  // STDIOBUS_ECHO_KERNEL_HPP
