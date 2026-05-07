/*
 * @license
 * Copyright 2026-present Raman Marozau, raman@stdiobus.com
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file bench_bus.cpp
 * @brief Benchmarks for stdio Bus C++ SDK
 *
 * Measures overhead of the C++ wrapper layer over the C kernel.
 * Run with: ./build/benchmarks/stdiobus_bench
 */

#include <benchmark/benchmark.h>
#include <stdiobus/error.hpp>
#include <stdiobus/types.hpp>
#include <stdiobus/version.hpp>

// Benchmark: Error construction (no allocation)
static void BM_ErrorConstruction(benchmark::State& state) {
    for (auto _ : state) {
        auto err = stdiobus::Error(stdiobus::ErrorCode::Ok);
        benchmark::DoNotOptimize(err);
    }
}
BENCHMARK(BM_ErrorConstruction);

// Benchmark: Error construction with message (allocation)
static void BM_ErrorWithMessage(benchmark::State& state) {
    for (auto _ : state) {
        auto err = stdiobus::Error(stdiobus::ErrorCode::Config, "configuration file not found");
        benchmark::DoNotOptimize(err);
    }
}
BENCHMARK(BM_ErrorWithMessage);

// Benchmark: Error code name lookup
static void BM_ErrorCodeName(benchmark::State& state) {
    for (auto _ : state) {
        auto name = stdiobus::error_code_name(stdiobus::ErrorCode::Timeout);
        benchmark::DoNotOptimize(name);
    }
}
BENCHMARK(BM_ErrorCodeName);

// Benchmark: State name lookup
static void BM_StateName(benchmark::State& state) {
    for (auto _ : state) {
        auto name = stdiobus::state_name(stdiobus::State::Running);
        benchmark::DoNotOptimize(name);
    }
}
BENCHMARK(BM_StateName);

// Benchmark: is_retryable check
static void BM_IsRetryable(benchmark::State& state) {
    auto err = stdiobus::Error(stdiobus::ErrorCode::Again);
    for (auto _ : state) {
        auto retryable = err.is_retryable();
        benchmark::DoNotOptimize(retryable);
    }
}
BENCHMARK(BM_IsRetryable);

// Benchmark: Version check
static void BM_VersionCheck(benchmark::State& state) {
    for (auto _ : state) {
        auto v = stdiobus::version();
        benchmark::DoNotOptimize(v);
    }
}
BENCHMARK(BM_VersionCheck);

// Benchmark: Duration conversion
static void BM_DurationConversion(benchmark::State& state) {
    auto dur = std::chrono::seconds(5);
    for (auto _ : state) {
        auto ms = stdiobus::to_ms(dur);
        benchmark::DoNotOptimize(ms);
    }
}
BENCHMARK(BM_DurationConversion);
