/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bus.cpp
 * @brief Implementation of stdiobus::Bus class
 *
 * Bus::Impl delegates all operations to an IKernel instance.
 * No direct C API calls at the Bus level — CKernel owns its own trampolines.
 */

#include <stdiobus/bus.hpp>
#include <stdiobus/echo_kernel.hpp>
#include <stdiobus/kernel.hpp>
#include <stdiobus/version.hpp>

#ifdef STDIOBUS_HAS_C_KERNEL
#include <stdiobus/c_kernel.hpp>
#endif

#include <fstream>
#include <sstream>
#include <string>

namespace stdiobus {
inline namespace v1 {

// ============================================================================
// Internal helpers
// ============================================================================

/**
 * @brief Serialize Options → JSON string for the kernel factory
 *
 * Priority:
 * 1. If config_json is set, use it directly
 * 2. If config_path is set, read file contents
 * 3. Otherwise, return "{}" (minimal empty config)
 */
static std::string options_to_json(const Options& opts) {
    if (!opts.config_json.empty()) {
        return opts.config_json;
    }
    if (!opts.config_path.empty()) {
        std::ifstream file(opts.config_path);
        if (file.is_open()) {
            std::ostringstream ss;
            ss << file.rdbuf();
            return ss.str();
        }
        // File not found — return empty, let validate_config() catch it
        return "{}";
    }
    return "{}";
}

/**
 * @brief Wire Options callbacks into a KernelCallbacks struct
 */
static KernelCallbacks make_kernel_callbacks(const Options& opts) {
    KernelCallbacks cbs;
    cbs.on_message = opts.on_message;
    cbs.on_error = opts.on_error;
    cbs.on_log = opts.on_log;
    cbs.on_worker = opts.on_worker;
    cbs.on_client_connect = opts.on_client_connect;
    cbs.on_client_disconnect = opts.on_client_disconnect;
    return cbs;
}

// ============================================================================
// Bus::Impl
// ============================================================================

/**
 * @brief Internal implementation struct
 *
 * Holds the abstract kernel instance and configuration.
 * No static callback trampolines — callbacks go directly to KernelCallbacks.
 */
struct Bus::Impl {
    std::unique_ptr<IKernel> kernel_;
    KernelFactory factory_;
    Options options;
    std::string json_config_;
};

// ============================================================================
// Constructors
// ============================================================================

Bus::Bus(std::string_view config_path) {
    Options opts;
    opts.config_path = std::string(config_path);
    *this = Bus(std::move(opts));
}

Bus::Bus(Options options) : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
    impl_->factory_ = default_kernel_factory();

    // Serialize Options → JSON
    impl_->json_config_ = options_to_json(impl_->options);

    // Create kernel via factory
    try {
        impl_->kernel_ = impl_->factory_(impl_->json_config_);
    } catch (...) {
        impl_->kernel_ = nullptr;
    }

    if (!impl_->kernel_) {
        // Factory failed — Bus is in invalid state
        return;
    }

    // Check interface version compatibility
    if (impl_->kernel_->interface_version() > KERNEL_INTERFACE_VERSION) {
        impl_->kernel_.reset();
        return;
    }

    // Validate config
    if (auto err = impl_->kernel_->validate_config(impl_->json_config_); err) {
        impl_->kernel_.reset();
        return;
    }

    // Wire callbacks directly from Options (no trampolines at Bus level)
    impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
}

Bus::Bus(Options options, KernelFactory factory) : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
    impl_->factory_ = std::move(factory);

    // Serialize Options → JSON
    impl_->json_config_ = options_to_json(impl_->options);

    // Create kernel via factory
    try {
        impl_->kernel_ = impl_->factory_(impl_->json_config_);
    } catch (...) {
        impl_->kernel_ = nullptr;
    }

    if (!impl_->kernel_) {
        return;
    }

    // Check interface version compatibility
    if (impl_->kernel_->interface_version() > KERNEL_INTERFACE_VERSION) {
        impl_->kernel_.reset();
        return;
    }

    // Validate config
    if (auto err = impl_->kernel_->validate_config(impl_->json_config_); err) {
        impl_->kernel_.reset();
        return;
    }

    // Wire callbacks
    impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
}

Bus::Bus(Options options, std::unique_ptr<IKernel> kernel) : impl_(std::make_unique<Impl>()) {
    impl_->options = std::move(options);
    impl_->kernel_ = std::move(kernel);

    if (!impl_->kernel_) {
        return;
    }

    // Check interface version compatibility
    if (impl_->kernel_->interface_version() > KERNEL_INTERFACE_VERSION) {
        impl_->kernel_.reset();
        return;
    }

    // Typed path: kernel is pre-constructed and assumed valid.
    // No validate_config() call — the kernel is ready by construction.
    // Just wire callbacks.
    impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
}

// ============================================================================
// Destructor and move
// ============================================================================

Bus::~Bus() {
    // IKernel destructor handles cleanup (CKernel stops + destroys C handle)
}

Bus::Bus(Bus&& other) noexcept : impl_(std::move(other.impl_)) {}

Bus& Bus::operator=(Bus&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
    }
    return *this;
}

// ============================================================================
// Lifecycle
// ============================================================================

Error Bus::start() {
    if (!impl_ || !impl_->kernel_) {
        return Error(ErrorCode::Invalid, "Bus not initialized");
    }
    return impl_->kernel_->start();
}

int Bus::step(Duration timeout) {
    if (!impl_ || !impl_->kernel_) {
        return static_cast<int>(ErrorCode::Invalid);
    }
    return impl_->kernel_->step(static_cast<int>(timeout.count()));
}

Error Bus::stop(std::chrono::seconds timeout) {
    if (!impl_ || !impl_->kernel_) {
        return Error(ErrorCode::Invalid, "Bus not initialized");
    }
    return impl_->kernel_->stop(static_cast<int>(timeout.count()));
}

// ============================================================================
// Messaging
// ============================================================================

Error Bus::send(std::string_view message) {
    if (!impl_ || !impl_->kernel_) {
        return Error(ErrorCode::Invalid, "Bus not initialized");
    }
    return impl_->kernel_->ingest(message.data(), message.size());
}

// ============================================================================
// State Queries
// ============================================================================

State Bus::state() const {
    if (!impl_ || !impl_->kernel_) {
        return State::Stopped;
    }
    return impl_->kernel_->state();
}

Stats Bus::stats() const {
    if (!impl_ || !impl_->kernel_) {
        return Stats{};
    }
    return impl_->kernel_->stats();
}

int Bus::worker_count() const {
    if (!impl_ || !impl_->kernel_)
        return 0;
    return impl_->kernel_->worker_count();
}

int Bus::session_count() const {
    if (!impl_ || !impl_->kernel_)
        return 0;
    return impl_->kernel_->session_count();
}

int Bus::pending_count() const {
    if (!impl_ || !impl_->kernel_)
        return 0;
    return impl_->kernel_->pending_count();
}

int Bus::client_count() const {
    if (!impl_ || !impl_->kernel_)
        return 0;
    return impl_->kernel_->client_count();
}

int Bus::poll_fd() const {
    if (!impl_ || !impl_->kernel_)
        return -1;
    return impl_->kernel_->poll_fd();
}

// ============================================================================
// Callbacks
// ============================================================================

void Bus::on_message(MessageCallback cb) {
    if (impl_) {
        impl_->options.on_message = std::move(cb);
        if (impl_->kernel_) {
            impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
        }
    }
}

void Bus::on_error(ErrorCallback cb) {
    if (impl_) {
        impl_->options.on_error = std::move(cb);
        if (impl_->kernel_) {
            impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
        }
    }
}

void Bus::on_log(LogCallback cb) {
    if (impl_) {
        impl_->options.on_log = std::move(cb);
        if (impl_->kernel_) {
            impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
        }
    }
}

void Bus::on_worker(WorkerCallback cb) {
    if (impl_) {
        impl_->options.on_worker = std::move(cb);
        if (impl_->kernel_) {
            impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
        }
    }
}

void Bus::on_client_connect(ClientConnectCallback cb) {
    if (impl_) {
        impl_->options.on_client_connect = std::move(cb);
        if (impl_->kernel_) {
            impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
        }
    }
}

void Bus::on_client_disconnect(ClientDisconnectCallback cb) {
    if (impl_) {
        impl_->options.on_client_disconnect = std::move(cb);
        if (impl_->kernel_) {
            impl_->kernel_->set_callbacks(make_kernel_callbacks(impl_->options));
        }
    }
}

// ============================================================================
// Advanced
// ============================================================================

stdio_bus_t* Bus::raw_handle() const {
    if (!impl_ || !impl_->kernel_) {
        return nullptr;
    }
#ifdef STDIOBUS_HAS_C_KERNEL
    // Dynamic cast to CKernel to get the underlying C handle
    if (auto* ck = dynamic_cast<CKernel*>(impl_->kernel_.get())) {
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
        return ck->raw_handle();
#pragma GCC diagnostic pop
    }
#endif
    return nullptr;  // Non-CKernel → nullptr
}

Bus::operator bool() const {
    return impl_ != nullptr && impl_->kernel_ != nullptr;
}

// ============================================================================
// Free functions
// ============================================================================

KernelFactory default_kernel_factory() noexcept {
#ifdef STDIOBUS_HAS_C_KERNEL
    return c_kernel_factory();
#else
    return echo_kernel_factory();
#endif
}

bool kernel_compatible() noexcept {
#ifdef STDIOBUS_HAS_C_KERNEL
    return STDIO_BUS_EMBED_API_VERSION == STDIOBUS_KERNEL_API_VERSION;
#else
    // Without C kernel, compatibility is always true (EchoKernel is built-in)
    return true;
#endif
}

}  // namespace v1
}  // namespace stdiobus
