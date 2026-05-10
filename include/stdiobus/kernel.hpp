/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file kernel.hpp
 * @brief Abstract kernel interface for stdio Bus implementations
 *
 * This header defines the integration contract for bus kernel implementations.
 * The C++ SDK facade (Bus, AsyncBus) operates against this interface, allowing
 * pluggable protocol/transport backends.
 *
 * Reference implementation: CKernel (wraps libstdio_bus.a C kernel)
 * Built-in alternative: EchoKernel (in-process loopback for testing)
 *
 * Configuration crosses the SDK boundary exclusively as JSON (std::string_view).
 * Each kernel implementation owns its config parsing and validation internally
 * via the validate_config() pure virtual method.
 *
 * Design: Abstract class with explicit versioning.
 * New pure virtual methods may be added with a version bump.
 */

#ifndef STDIOBUS_KERNEL_HPP
#define STDIOBUS_KERNEL_HPP

#include <stdiobus/error.hpp>
#include <stdiobus/types.hpp>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace stdiobus {
inline namespace v1 {

/**
 * @brief Kernel interface version
 *
 * Incremented when new operations are added to KernelOps.
 * Implementations report their version; the facade checks compatibility.
 */
constexpr int KERNEL_INTERFACE_VERSION = 1;

// ============================================================================
// Kernel Callbacks (host ← kernel direction)
// ============================================================================

/**
 * @brief Callbacks that the kernel invokes to deliver events to the host
 *
 * Set by the facade before start(). Kernel implementations MUST invoke
 * these from the same thread that calls step().
 */
struct KernelCallbacks {
    MessageCallback on_message;
    ErrorCallback on_error;
    LogCallback on_log;
    WorkerCallback on_worker;
    ClientConnectCallback on_client_connect;
    ClientDisconnectCallback on_client_disconnect;
};

// ============================================================================
// Abstract Kernel Interface
// ============================================================================

/**
 * @brief Abstract interface for bus kernel implementations
 *
 * This is the integration contract between the C++ SDK facade and the
 * underlying bus engine. Implementations provide process management,
 * message routing, and protocol handling.
 *
 * Lifecycle:
 *   1. Construct (via factory) with JSON config string
 *   2. set_callbacks() — register event handlers
 *   3. start() — spawn workers / initialize transport
 *   4. step() in a loop — pump I/O
 *   5. stop() — graceful shutdown
 *   6. Destructor — cleanup
 *
 * Threading: All methods must be called from a single thread.
 * The kernel must not invoke callbacks from other threads.
 *
 * Error model: Methods return Error (check with if(err)).
 * Fatal errors are also reported via on_error callback.
 */
class IKernel {
public:
    virtual ~IKernel() = default;

    // ========== Interface metadata ==========

    /**
     * @brief Return the kernel interface version this implementation supports
     *
     * Must return KERNEL_INTERFACE_VERSION or lower.
     * The facade uses this to detect capability mismatches.
     */
    virtual int interface_version() const noexcept = 0;

    /**
     * @brief Human-readable name of this kernel implementation
     *
     * Examples: "stdio_bus_c_kernel", "echo_kernel", "custom_mcp_v2"
     */
    virtual std::string_view name() const noexcept = 0;

    // ========== Configuration Validation ==========

    /**
     * @brief Validate raw JSON config against this kernel's schema
     *
     * This IS the type definition of the config — it defines what JSON is
     * acceptable for this kernel implementation. Called by the facade BEFORE
     * start(). Each kernel parses and validates internally.
     *
     * @param json  Raw JSON configuration string
     * @return Error::ok() if valid, ErrorCode::Config with message if invalid
     */
    [[nodiscard]] virtual Error validate_config(std::string_view json) const = 0;

    // ========== Callbacks ==========

    /**
     * @brief Register callbacks for kernel → host events
     *
     * Must be called before start(). May be called again to update callbacks
     * while stopped.
     */
    virtual void set_callbacks(const KernelCallbacks& callbacks) = 0;

    // ========== Lifecycle ==========

    /**
     * @brief Start the kernel (spawn workers, bind listeners)
     * @return Error on failure
     */
    [[nodiscard]] virtual Error start() = 0;

    /**
     * @brief Process pending I/O (non-blocking event pump)
     *
     * @param timeout_ms Maximum wait time:
     *   - 0: non-blocking, return immediately
     *   - >0: wait up to this many milliseconds
     *   - -1: block until event
     * @return Number of events processed, or negative error code
     */
    virtual int step(int timeout_ms) = 0;

    /**
     * @brief Initiate graceful shutdown
     *
     * @param timeout_sec Maximum time to wait for workers to exit
     * @return Error on failure
     */
    [[nodiscard]] virtual Error stop(int timeout_sec) = 0;

    // ========== Messaging ==========

    /**
     * @brief Send a message into the bus (host → workers)
     *
     * @param message JSON-RPC message
     * @param len Message length in bytes
     * @return Error on failure
     */
    [[nodiscard]] virtual Error ingest(const char* message, size_t len) = 0;

    // ========== State Queries ==========

    /** @brief Get current bus state */
    virtual State state() const noexcept = 0;

    /** @brief Get number of active workers */
    virtual int worker_count() const noexcept = 0;

    /** @brief Get number of active sessions */
    virtual int session_count() const noexcept = 0;

    /** @brief Get number of pending requests */
    virtual int pending_count() const noexcept = 0;

    /** @brief Get number of connected clients (TCP/Unix modes) */
    virtual int client_count() const noexcept = 0;

    /**
     * @brief Get poll file descriptor for event loop integration
     * @return fd suitable for poll/epoll/kqueue, or -1 if not available
     */
    virtual int poll_fd() const noexcept = 0;

    /** @brief Get statistics snapshot */
    virtual Stats stats() const noexcept = 0;

    // ========== Embedded Worker Support ==========

    /**
     * @brief Register an embedded worker with pre-existing socketpair fds
     *
     * For language runtime integration (N-API, Python C extension, etc.)
     * The kernel takes ownership of the passed fds.
     *
     * @param fd_to_worker FD for kernel to write to embedded worker
     * @param fd_from_worker FD for kernel to read from embedded worker
     * @param pool_id Pool identifier for this embedded worker
     * @return Worker ID (>= 0) on success, or negative error code
     */
    virtual int register_embedded_worker(int fd_to_worker,
                                         int fd_from_worker,
                                         std::string_view pool_id) = 0;

    /**
     * @brief Unregister an embedded worker
     *
     * Removes fds from event loop and closes kernel-side fds.
     * Idempotent.
     *
     * @param worker_id Worker ID returned by register_embedded_worker
     * @return Error on failure
     */
    [[nodiscard]] virtual Error unregister_embedded_worker(int worker_id) = 0;
};

// ============================================================================
// Kernel Factory
// ============================================================================

/**
 * @brief Factory function type for creating kernel instances
 *
 * Implementations register a factory that constructs their kernel
 * from a JSON configuration string. No typed config struct crosses
 * the SDK boundary — JSON is the universal config format.
 */
using KernelFactory = std::function<std::unique_ptr<IKernel>(std::string_view json_config)>;

/**
 * @brief Get the default kernel factory
 *
 * Returns the factory for the reference C kernel implementation when
 * STDIOBUS_HAS_C_KERNEL is defined, otherwise returns the EchoKernel factory.
 *
 * Users can override this by passing a custom factory to BusBuilder.
 */
KernelFactory default_kernel_factory() noexcept;

}  // namespace v1
}  // namespace stdiobus

#endif  // STDIOBUS_KERNEL_HPP
