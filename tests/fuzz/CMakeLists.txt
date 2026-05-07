# Fuzz testing targets (requires Clang with libFuzzer)
#
# Build:
#   cmake -S . -B build-fuzz -DCMAKE_CXX_COMPILER=clang++ \
#       -DSTDIOBUS_BUILD_FUZZ=ON -DCMAKE_BUILD_TYPE=Debug
#   cmake --build build-fuzz --target fuzz_message_parse
#
# Run:
#   ./build-fuzz/tests/fuzz/fuzz_message_parse corpus/ -max_total_time=60

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_executable(fuzz_message_parse fuzz_message_parse.cpp)
    target_link_libraries(fuzz_message_parse PRIVATE stdiobus)
    target_compile_options(fuzz_message_parse PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_message_parse PRIVATE -fsanitize=fuzzer,address,undefined)
else()
    message(STATUS "Fuzz targets require Clang — skipping")
endif()
