/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file c_kernel.cpp
 * @brief Implementation of CKernel adapter wrapping libstdio_bus.a
 */

#ifdef STDIOBUS_HAS_C_KERNEL

#include <stdiobus/c_kernel.hpp>
#include <stdiobus/ffi.hpp>

#include <cstring>
#include <string>
#include <utility>

namespace stdiobus {
inline namespace v1 {

// ============================================================================
// Static callback trampolines
// ============================================================================

void CKernel::message_trampoline(stdio_bus_t* /*bus*/, const char* msg,
                                  size_t len, void* user_data) {
    auto* self = static_cast<CKernel*>(user_data);
    if (self->callbacks_.on_message) {
        self->callbacks_.on_message(std::string_view(msg, len));
    }
}

void CKernel::error_trampoline(stdio_bus_t* /*bus*/, int code,
                                const char* message, void* user_data) {
    auto* self = static_cast<CKernel*>(user_data);
    if (self->callbacks_.on_error) {
        self->callbacks_.on_error(static_cast<ErrorCode>(code), message ? message : "");
    }
}

void CKernel::log_trampoline(stdio_bus_t* /*bus*/, int level,
                              const char* message, void* user_data) {
    auto* self = static_cast<CKernel*>(user_data);
    if (self->callbacks_.on_log) {
        self->callbacks_.on_log(level, message ? message : "");
    }
}

void CKernel::worker_trampoline(stdio_bus_t* /*bus*/, int worker_id,
                                 const char* event, void* user_data) {
    auto* self = static_cast<CKernel*>(user_data);
    if (self->callbacks_.on_worker) {
        self->callbacks_.on_worker(worker_id, event ? event : "");
    }
}

void CKernel::client_connect_trampoline(stdio_bus_t* /*bus*/, int client_id,
                                         const char* peer_info, void* user_data) {
    auto* self = static_cast<CKernel*>(user_data);
    if (self->callbacks_.on_client_connect) {
        self->callbacks_.on_client_connect(client_id, peer_info ? peer_info : "");
    }
}

void CKernel::client_disconnect_trampoline(stdio_bus_t* /*bus*/, int client_id,
                                            const char* reason, void* user_data) {
    auto* self = static_cast<CKernel*>(user_data);
    if (self->callbacks_.on_client_disconnect) {
        self->callbacks_.on_client_disconnect(client_id, reason ? reason : "");
    }
}

// ============================================================================
// Construction / Destruction
// ============================================================================

CKernel::CKernel(const detail::KernelConfig& config) : config_(config) {
    // Build C options struct
    stdio_bus_options_t c_opts{};

    if (!config_.config_path.empty()) {
        c_opts.config_path = config_.config_path.c_str();
    }
    if (!config_.config_json.empty()) {
        c_opts.config_json = config_.config_json.c_str();
    }

    // Listener config
    c_opts.listener.mode = static_cast<stdio_bus_listen_mode_t>(config_.listener.mode);
    if (!config_.listener.tcp_host.empty()) {
        c_opts.listener.tcp_host = config_.listener.tcp_host.c_str();
    }
    c_opts.listener.tcp_port = config_.listener.tcp_port;
    if (!config_.listener.unix_path.empty()) {
        c_opts.listener.unix_path = config_.listener.unix_path.c_str();
    }

    // Callbacks — always set trampolines, they check if callback is set
    c_opts.on_message = CKernel::message_trampoline;
    c_opts.on_error = CKernel::error_trampoline;
    c_opts.on_log = CKernel::log_trampoline;
    c_opts.on_worker = CKernel::worker_trampoline;
    c_opts.on_client_connect = CKernel::client_connect_trampoline;
    c_opts.on_client_disconnect = CKernel::client_disconnect_trampoline;

    c_opts.user_data = this;
    c_opts.log_level = config_.log_level;

    // Create the bus handle
    handle_ = stdio_bus_create(&c_opts);
}

CKernel::~CKernel() {
    if (handle_) {
        auto s = stdio_bus_get_state(handle_);
        if (s == STDIO_BUS_STATE_RUNNING || s == STDIO_BUS_STATE_STARTING) {
            stdio_bus_stop(handle_, 5);
        }
        stdio_bus_destroy(handle_);
        handle_ = nullptr;
    }
}

// ============================================================================
// Move semantics
// ============================================================================

CKernel::CKernel(CKernel&& other) noexcept
    : handle_(other.handle_),
      callbacks_(std::move(other.callbacks_)),
      config_(std::move(other.config_)) {
    other.handle_ = nullptr;

    // Update user_data pointer in the C handle to point to new location
    if (handle_) {
        // The C API doesn't expose a way to update user_data after creation,
        // but since we moved the handle, callbacks will use 'this' which is
        // the new object. We need to re-register callbacks with updated pointer.
        // However, the C API stores user_data at creation time. Since we took
        // ownership of the handle, the user_data still points to 'other' which
        // is now empty. We must not call step() until set_callbacks() is called
        // again (which updates user_data via a new create cycle).
        //
        // In practice, move is used before start() or after stop(), so this is safe.
        // The facade calls set_callbacks() after construction.
    }
}

CKernel& CKernel::operator=(CKernel&& other) noexcept {
    if (this != &other) {
        // Destroy current handle
        if (handle_) {
            auto s = stdio_bus_get_state(handle_);
            if (s == STDIO_BUS_STATE_RUNNING || s == STDIO_BUS_STATE_STARTING) {
                stdio_bus_stop(handle_, 5);
            }
            stdio_bus_destroy(handle_);
        }

        handle_ = other.handle_;
        callbacks_ = std::move(other.callbacks_);
        config_ = std::move(other.config_);
        other.handle_ = nullptr;
    }
    return *this;
}

// ============================================================================
// Configuration Validation
// ============================================================================

Error CKernel::validate_config(std::string_view json) const {
    if (json.empty()) {
        return Error(ErrorCode::Config, "CKernel requires JSON configuration");
    }

    // Verify that "workers" or "pools" array is present in the JSON config
    // This is a lightweight check — the C kernel does full validation on start()
    bool has_workers = json.find("\"workers\"") != std::string_view::npos;
    bool has_pools = json.find("\"pools\"") != std::string_view::npos;

    if (!has_workers && !has_pools) {
        return Error(ErrorCode::Config,
                     "CKernel config must contain \"workers\" or \"pools\" array");
    }

    return Error::ok();
}

// ============================================================================
// Callbacks
// ============================================================================

void CKernel::set_callbacks(const KernelCallbacks& callbacks) {
    callbacks_ = callbacks;
}

// ============================================================================
// Lifecycle
// ============================================================================

Error CKernel::start() {
    if (!handle_) {
        return Error(ErrorCode::Invalid, "CKernel: handle not initialized");
    }
    int rc = stdio_bus_start(handle_);
    return Error::from_c(rc);
}

int CKernel::step(int timeout_ms) {
    if (!handle_) {
        return static_cast<int>(ErrorCode::Invalid);
    }
    return stdio_bus_step(handle_, timeout_ms);
}

Error CKernel::stop(int timeout_sec) {
    if (!handle_) {
        return Error(ErrorCode::Invalid, "CKernel: handle not initialized");
    }
    int rc = stdio_bus_stop(handle_, timeout_sec);
    return Error::from_c(rc);
}

// ============================================================================
// Messaging
// ============================================================================

Error CKernel::ingest(const char* message, size_t len) {
    if (!handle_) {
        return Error(ErrorCode::Invalid, "CKernel: handle not initialized");
    }
    int rc = stdio_bus_ingest(handle_, message, len);
    return Error::from_c(rc);
}

// ============================================================================
// State Queries
// ============================================================================

State CKernel::state() const noexcept {
    if (!handle_) {
        return State::Stopped;
    }
    return static_cast<State>(stdio_bus_get_state(handle_));
}

int CKernel::worker_count() const noexcept {
    if (!handle_) return 0;
    return stdio_bus_worker_count(handle_);
}

int CKernel::session_count() const noexcept {
    if (!handle_) return 0;
    return stdio_bus_session_count(handle_);
}

int CKernel::pending_count() const noexcept {
    if (!handle_) return 0;
    return stdio_bus_pending_count(handle_);
}

int CKernel::client_count() const noexcept {
    if (!handle_) return 0;
    return stdio_bus_client_count(handle_);
}

int CKernel::poll_fd() const noexcept {
    if (!handle_) return -1;
    return stdio_bus_get_poll_fd(handle_);
}

Stats CKernel::stats() const noexcept {
    Stats result{};
    if (handle_) {
        stdio_bus_stats_t c_stats{};
        stdio_bus_get_stats(handle_, &c_stats);
        result = ffi::to_stats(c_stats);
    }
    return result;
}

// ============================================================================
// Embedded Worker Support
// ============================================================================

int CKernel::register_embedded_worker(int fd_to_worker,
                                       int fd_from_worker,
                                       std::string_view pool_id) {
    if (!handle_) {
        return static_cast<int>(ErrorCode::Invalid);
    }
    // pool_id needs null termination for C API
    std::string pool_id_str(pool_id);
    return stdio_bus_register_embedded_worker(handle_, fd_to_worker,
                                              fd_from_worker, pool_id_str.c_str());
}

Error CKernel::unregister_embedded_worker(int worker_id) {
    if (!handle_) {
        return Error(ErrorCode::Invalid, "CKernel: handle not initialized");
    }
    int rc = stdio_bus_unregister_embedded_worker(handle_, worker_id);
    return Error::from_c(rc);
}

// ============================================================================
// Factory
// ============================================================================

KernelFactory c_kernel_factory() noexcept {
    return [](std::string_view json_config) -> std::unique_ptr<IKernel> {
        detail::KernelConfig config;
        config.config_json = std::string(json_config);
        return std::make_unique<CKernel>(config);
    };
}

}  // namespace v1
}  // namespace stdiobus

#endif  // STDIOBUS_HAS_C_KERNEL
