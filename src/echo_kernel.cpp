/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file echo_kernel.cpp
 * @brief Implementation of EchoKernel (in-process loopback)
 */

#include <stdiobus/echo_kernel.hpp>

namespace stdiobus {
inline namespace v1 {

EchoKernel::EchoKernel(const detail::KernelConfig& config) : config_(config) {}

Error EchoKernel::validate_config(std::string_view /*json*/) const {
    // Identity validation — EchoKernel accepts any JSON config
    return Error::ok();
}

void EchoKernel::set_callbacks(const KernelCallbacks& callbacks) {
    callbacks_ = callbacks;
}

Error EchoKernel::start() {
    if (state_ != State::Created) {
        return Error(ErrorCode::State, "EchoKernel: can only start from Created state");
    }
    state_ = State::Starting;

    if (callbacks_.on_log) {
        callbacks_.on_log(1, "EchoKernel: started (loopback mode)");
    }

    state_ = State::Running;
    return Error::ok();
}

int EchoKernel::step(int /*timeout_ms*/) {
    if (state_ != State::Running) {
        return 0;
    }

    int delivered = 0;
    while (!pending_messages_.empty()) {
        auto msg = std::move(pending_messages_.front());
        pending_messages_.pop_front();

        stats_.messages_out++;
        stats_.bytes_out += msg.size();

        if (callbacks_.on_message) {
            callbacks_.on_message(std::string_view(msg));
        }
        delivered++;
    }
    return delivered;
}

Error EchoKernel::stop(int /*timeout_sec*/) {
    if (state_ == State::Stopped) {
        return Error::ok();
    }
    if (state_ != State::Running && state_ != State::Starting) {
        return Error(ErrorCode::State, "EchoKernel: invalid state for stop");
    }

    state_ = State::Stopping;

    if (callbacks_.on_log) {
        callbacks_.on_log(1, "EchoKernel: stopping");
    }

    // Drain pending messages
    pending_messages_.clear();

    state_ = State::Stopped;
    return Error::ok();
}

Error EchoKernel::ingest(const char* message, size_t len) {
    if (state_ != State::Running) {
        return Error(ErrorCode::State, "EchoKernel: can only ingest in Running state");
    }
    if (!message || len == 0) {
        return Error(ErrorCode::Invalid, "EchoKernel: empty message");
    }

    stats_.messages_in++;
    stats_.bytes_in += len;

    pending_messages_.emplace_back(message, len);
    return Error::ok();
}

int EchoKernel::pending_count() const noexcept {
    return static_cast<int>(pending_messages_.size());
}

int EchoKernel::register_embedded_worker(int /*fd_to_worker*/,
                                          int /*fd_from_worker*/,
                                          std::string_view /*pool_id*/) {
    // Echo kernel does not support embedded workers
    return static_cast<int>(ErrorCode::Invalid);
}

Error EchoKernel::unregister_embedded_worker(int /*worker_id*/) {
    return Error(ErrorCode::Invalid, "EchoKernel: embedded workers not supported");
}

KernelFactory echo_kernel_factory() noexcept {
    return [](std::string_view json_config) -> std::unique_ptr<IKernel> {
        detail::KernelConfig config;
        config.config_json = std::string(json_config);
        return std::make_unique<EchoKernel>(config);
    };
}

}  // namespace v1
}  // namespace stdiobus
