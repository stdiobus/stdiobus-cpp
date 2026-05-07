include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)
set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(benchmark)

add_executable(stdiobus_bench
    bench_bus.cpp
)

target_link_libraries(stdiobus_bench PRIVATE
    stdiobus
    benchmark::benchmark
    benchmark::benchmark_main
)
