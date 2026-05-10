/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file test_kernel_properties.cpp
 * @brief Property-based tests for EchoKernel invariants
 *
 * Uses custom random generators with GoogleTest to verify universal
 * properties hold across randomized inputs. Fixed seed ensures
 * reproducibility while covering diverse message content and
 * operation sequences.
 *
 * Validates: Requirements 9.5, 9.6
 */

#include <stdiobus/echo_kernel.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

using namespace stdiobus;

namespace {

// =============================================================================
// Random generators
// =============================================================================

/// Fixed seed for reproducibility
constexpr uint64_t SEED = 42;

/// Number of iterations per property test
constexpr int NUM_ITERATIONS = 200;

/// Maximum message length for generated messages
constexpr size_t MAX_MSG_LEN = 4096;

/**
 * @brief Generate a random message with varying content types
 *
 * Produces messages containing:
 * - ASCII text
 * - Binary data (all byte values 0x00-0xFF)
 * - Embedded null bytes
 * - UTF-8 multi-byte sequences
 * - Single-byte messages
 */
std::string generate_random_message(std::mt19937_64& rng) {
    std::uniform_int_distribution<size_t> len_dist(1, MAX_MSG_LEN);
    std::uniform_int_distribution<int> type_dist(0, 4);
    std::uniform_int_distribution<int> byte_dist(0, 255);

    size_t len = len_dist(rng);
    int msg_type = type_dist(rng);

    std::string msg;
    msg.reserve(len);

    switch (msg_type) {
        case 0: {
            // Pure ASCII printable
            std::uniform_int_distribution<int> ascii_dist(32, 126);
            for (size_t i = 0; i < len; ++i) {
                msg.push_back(static_cast<char>(ascii_dist(rng)));
            }
            break;
        }
        case 1: {
            // Binary data (full byte range)
            for (size_t i = 0; i < len; ++i) {
                msg.push_back(static_cast<char>(byte_dist(rng)));
            }
            break;
        }
        case 2: {
            // ASCII with embedded null bytes
            std::uniform_int_distribution<int> ascii_dist(32, 126);
            std::uniform_int_distribution<int> null_chance(0, 9);
            for (size_t i = 0; i < len; ++i) {
                if (null_chance(rng) == 0) {
                    msg.push_back('\0');
                } else {
                    msg.push_back(static_cast<char>(ascii_dist(rng)));
                }
            }
            break;
        }
        case 3: {
            // UTF-8 multi-byte sequences mixed with ASCII
            size_t i = 0;
            while (i < len) {
                std::uniform_int_distribution<int> choice(0, 3);
                int c = choice(rng);
                if (c == 0 && i + 2 <= len) {
                    // 2-byte UTF-8 (U+0080 to U+07FF)
                    std::uniform_int_distribution<int> cp_dist(0x80, 0x7FF);
                    int cp = cp_dist(rng);
                    msg.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                    msg.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    i += 2;
                } else if (c == 1 && i + 3 <= len) {
                    // 3-byte UTF-8 (U+0800 to U+FFFF)
                    std::uniform_int_distribution<int> cp_dist(0x800, 0xFFFF);
                    int cp = cp_dist(rng);
                    msg.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                    msg.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                    msg.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                    i += 3;
                } else {
                    // ASCII byte
                    std::uniform_int_distribution<int> ascii_dist(32, 126);
                    msg.push_back(static_cast<char>(ascii_dist(rng)));
                    i += 1;
                }
            }
            break;
        }
        case 4: {
            // JSON-like content
            std::string templates[] = {
                R"({"id":)", R"(,"method":")", R"(","params":{}})",
                R"({"jsonrpc":"2.0","result":)", R"(})"};
            for (size_t i = 0; i < len;) {
                std::uniform_int_distribution<size_t> t_dist(0, 4);
                const auto& t = templates[t_dist(rng)];
                size_t to_copy = std::min(t.size(), len - i);
                msg.append(t.data(), to_copy);
                i += to_copy;
            }
            msg.resize(len);
            break;
        }
    }

    return msg;
}

/// Helper to create a default EchoKernel instance
std::unique_ptr<EchoKernel> make_echo_kernel() {
    detail::KernelConfig config;
    config.config_json = R"({"test": true})";
    return std::make_unique<EchoKernel>(config);
}

/// Helper to start a kernel with an optional on_message callback
void start_kernel(EchoKernel& kernel, MessageCallback on_message = nullptr) {
    KernelCallbacks callbacks;
    callbacks.on_message = std::move(on_message);
    kernel.set_callbacks(callbacks);
    auto err = kernel.start();
    ASSERT_FALSE(err) << "start() failed: " << err.message();
}

/// Enum for random lifecycle operations
enum class Operation { Start, Stop, Ingest, Step };

}  // namespace

// =============================================================================
// Property 1: Round-trip conservation
// For ANY valid message string ingested, after step(),
// stats.messages_out == stats.messages_in
//
// Validates: Requirements 9.5
// =============================================================================

TEST(KernelProperties, RoundTripConservation) {
    std::mt19937_64 rng(SEED);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        SCOPED_TRACE("Iteration " + std::to_string(iter));

        auto kernel = make_echo_kernel();
        start_kernel(*kernel);

        // Generate 1-10 random messages per iteration
        std::uniform_int_distribution<int> count_dist(1, 10);
        int msg_count = count_dist(rng);

        for (int i = 0; i < msg_count; ++i) {
            std::string msg = generate_random_message(rng);
            auto err = kernel->ingest(msg.data(), msg.size());
            ASSERT_FALSE(err) << "ingest failed at message " << i;
        }

        kernel->step(0);

        auto s = kernel->stats();
        EXPECT_EQ(s.messages_out, s.messages_in)
            << "Round-trip conservation violated: messages_in=" << s.messages_in
            << " messages_out=" << s.messages_out;
    }
}

// =============================================================================
// Property 2: Pending count accuracy
// For ANY sequence of N ingest calls followed by one step(),
// pending_count() before step == N, after step == 0
//
// Validates: Requirements 9.6
// =============================================================================

TEST(KernelProperties, PendingCountAccuracy) {
    std::mt19937_64 rng(SEED + 1);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        SCOPED_TRACE("Iteration " + std::to_string(iter));

        auto kernel = make_echo_kernel();
        start_kernel(*kernel);

        std::uniform_int_distribution<int> count_dist(1, 20);
        int n = count_dist(rng);

        for (int i = 0; i < n; ++i) {
            std::string msg = generate_random_message(rng);
            auto err = kernel->ingest(msg.data(), msg.size());
            ASSERT_FALSE(err) << "ingest failed at message " << i;
        }

        EXPECT_EQ(kernel->pending_count(), n)
            << "pending_count before step should equal " << n;

        kernel->step(0);

        EXPECT_EQ(kernel->pending_count(), 0)
            << "pending_count after step should be 0";
    }
}

// =============================================================================
// Property 3: Byte-for-byte fidelity
// For ANY message ingested, the on_message callback receives
// byte-for-byte identical data
//
// Validates: Requirements 9.5
// =============================================================================

TEST(KernelProperties, ByteForByteFidelity) {
    std::mt19937_64 rng(SEED + 2);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        SCOPED_TRACE("Iteration " + std::to_string(iter));

        auto kernel = make_echo_kernel();

        std::vector<std::string> sent;
        std::vector<std::string> received;

        start_kernel(*kernel, [&](std::string_view msg) {
            received.emplace_back(msg);
        });

        // Generate 1-5 messages per iteration
        std::uniform_int_distribution<int> count_dist(1, 5);
        int msg_count = count_dist(rng);

        for (int i = 0; i < msg_count; ++i) {
            std::string msg = generate_random_message(rng);
            sent.push_back(msg);
            auto err = kernel->ingest(msg.data(), msg.size());
            ASSERT_FALSE(err) << "ingest failed at message " << i;
        }

        kernel->step(0);

        ASSERT_EQ(received.size(), sent.size())
            << "Received count mismatch";

        for (size_t i = 0; i < sent.size(); ++i) {
            SCOPED_TRACE("Message " + std::to_string(i) + " (len=" +
                         std::to_string(sent[i].size()) + ")");
            EXPECT_EQ(received[i].size(), sent[i].size())
                << "Length mismatch";
            EXPECT_EQ(received[i], sent[i])
                << "Content mismatch (byte-for-byte comparison failed)";
        }
    }
}

// =============================================================================
// Property 4: State machine robustness
// For ANY sequence of lifecycle operations, state transitions follow
// the state machine (no illegal transitions succeed)
//
// Validates: Requirements 9.5
// =============================================================================

TEST(KernelProperties, StateMachineRobustness) {
    std::mt19937_64 rng(SEED + 3);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        SCOPED_TRACE("Iteration " + std::to_string(iter));

        auto kernel = make_echo_kernel();
        KernelCallbacks callbacks;
        callbacks.on_message = [](std::string_view) {};
        kernel->set_callbacks(callbacks);

        // Generate a random sequence of operations
        std::uniform_int_distribution<int> op_dist(0, 3);
        std::uniform_int_distribution<int> seq_len_dist(5, 30);
        int seq_len = seq_len_dist(rng);

        for (int step = 0; step < seq_len; ++step) {
            State state_before = kernel->state();
            auto op = static_cast<Operation>(op_dist(rng));

            switch (op) {
                case Operation::Start: {
                    auto err = kernel->start();
                    if (state_before == State::Created) {
                        // start() should succeed from Created
                        EXPECT_FALSE(err)
                            << "start() should succeed from Created state";
                        EXPECT_EQ(kernel->state(), State::Running);
                    } else {
                        // start() should fail from any other state
                        EXPECT_TRUE(err)
                            << "start() should fail from state "
                            << static_cast<int>(state_before);
                        EXPECT_EQ(err.code(), ErrorCode::State);
                        // State should not change on failure
                        EXPECT_EQ(kernel->state(), state_before);
                    }
                    break;
                }
                case Operation::Stop: {
                    auto err = kernel->stop(0);
                    if (state_before == State::Running ||
                        state_before == State::Starting) {
                        // stop() should succeed from Running/Starting
                        EXPECT_FALSE(err)
                            << "stop() should succeed from Running/Starting";
                        EXPECT_EQ(kernel->state(), State::Stopped);
                    } else if (state_before == State::Stopped) {
                        // stop() is idempotent when already Stopped
                        EXPECT_FALSE(err)
                            << "stop() should be idempotent in Stopped state";
                        EXPECT_EQ(kernel->state(), State::Stopped);
                    } else {
                        // stop() from Created should fail
                        EXPECT_TRUE(err)
                            << "stop() should fail from state "
                            << static_cast<int>(state_before);
                        EXPECT_EQ(err.code(), ErrorCode::State);
                        EXPECT_EQ(kernel->state(), state_before);
                    }
                    break;
                }
                case Operation::Ingest: {
                    std::string msg = generate_random_message(rng);
                    auto err = kernel->ingest(msg.data(), msg.size());
                    if (state_before == State::Running) {
                        // ingest() should succeed in Running state
                        EXPECT_FALSE(err)
                            << "ingest() should succeed in Running state";
                    } else {
                        // ingest() should fail in non-Running state
                        EXPECT_TRUE(err)
                            << "ingest() should fail from state "
                            << static_cast<int>(state_before);
                        EXPECT_EQ(err.code(), ErrorCode::State);
                    }
                    // State should not change from ingest
                    EXPECT_EQ(kernel->state(), state_before);
                    break;
                }
                case Operation::Step: {
                    int result = kernel->step(0);
                    // step() should not crash regardless of state
                    // In non-Running state, it returns 0
                    if (state_before != State::Running) {
                        EXPECT_EQ(result, 0)
                            << "step() should return 0 in non-Running state";
                    } else {
                        EXPECT_GE(result, 0)
                            << "step() should return >= 0 in Running state";
                    }
                    // step() should not change state
                    EXPECT_EQ(kernel->state(), state_before);
                    break;
                }
            }
        }
    }
}

// =============================================================================
// Property 5: Byte accounting accuracy
// bytes_in == sum of all ingested message lengths
// bytes_out == sum of all delivered message lengths
//
// Validates: Requirements 9.5
// =============================================================================

TEST(KernelProperties, ByteAccountingAccuracy) {
    std::mt19937_64 rng(SEED + 4);

    for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
        SCOPED_TRACE("Iteration " + std::to_string(iter));

        auto kernel = make_echo_kernel();
        start_kernel(*kernel);

        std::uniform_int_distribution<int> count_dist(1, 15);
        int msg_count = count_dist(rng);

        uint64_t expected_bytes = 0;
        std::vector<size_t> msg_lengths;

        for (int i = 0; i < msg_count; ++i) {
            std::string msg = generate_random_message(rng);
            msg_lengths.push_back(msg.size());
            expected_bytes += msg.size();
            auto err = kernel->ingest(msg.data(), msg.size());
            ASSERT_FALSE(err) << "ingest failed at message " << i;
        }

        // Before step: bytes_in should equal sum of ingested lengths
        auto s_before = kernel->stats();
        EXPECT_EQ(s_before.bytes_in, expected_bytes)
            << "bytes_in mismatch before step";
        EXPECT_EQ(s_before.bytes_out, 0u)
            << "bytes_out should be 0 before step";

        kernel->step(0);

        // After step: bytes_out should equal bytes_in (echo)
        auto s_after = kernel->stats();
        EXPECT_EQ(s_after.bytes_in, expected_bytes)
            << "bytes_in changed after step";
        EXPECT_EQ(s_after.bytes_out, expected_bytes)
            << "bytes_out should equal sum of delivered message lengths";

        // Verify individual message lengths sum correctly
        uint64_t sum_lengths = 0;
        for (auto len : msg_lengths) {
            sum_lengths += len;
        }
        EXPECT_EQ(s_after.bytes_out, sum_lengths)
            << "bytes_out should equal sum of all message lengths";
    }
}
