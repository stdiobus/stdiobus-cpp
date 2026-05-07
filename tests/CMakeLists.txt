include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Unit test executable
add_executable(stdiobus_tests
    test_error.cpp
    test_types.cpp
    test_ffi.cpp
    test_bus.cpp
    test_async.cpp
)

target_link_libraries(stdiobus_tests PRIVATE
    stdiobus
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(stdiobus_tests)

# E2E test executable (requires libstdio_bus.a with real workers)
add_executable(stdiobus_e2e
    test_e2e.cpp
)

target_link_libraries(stdiobus_e2e PRIVATE
    stdiobus
    GTest::gtest_main
)

gtest_discover_tests(stdiobus_e2e
    TEST_PREFIX "E2E/"
    PROPERTIES LABELS "e2e"
)

# Kernel parity test executable
# Calls the SAME C functions as tests/test_main.c directly from C++.
# Ensures the SDK links correctly and kernel behavior is identical.
# NOTE: Requires full kernel source tree (internal headers). Skipped in standalone builds.
set(KERNEL_INTERNAL_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/../../../include/stdio_bus.h")

if(EXISTS "${KERNEL_INTERNAL_HEADER}")
    add_executable(stdiobus_kernel_parity
        test_kernel_parity.cpp
    )

    target_include_directories(stdiobus_kernel_parity PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../include"
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../src"
    )

    target_link_libraries(stdiobus_kernel_parity PRIVATE
        stdiobus
        GTest::gtest_main
    )

    # Kernel parity tests include kernel headers directly — need platform define
    if(APPLE)
        target_compile_definitions(stdiobus_kernel_parity PRIVATE STDIO_BUS_USE_KQUEUE)
    else()
        target_compile_definitions(stdiobus_kernel_parity PRIVATE STDIO_BUS_USE_EPOLL)
    endif()

    gtest_discover_tests(stdiobus_kernel_parity
        TEST_PREFIX "KernelParity/"
        PROPERTIES
            LABELS "kernel-parity"
    )
else()
    message(STATUS "Kernel parity tests skipped (internal headers not found)")
endif()

# =============================================================================
# Kernel Unit Tests — the ORIGINAL tests/test_main.c compiled and linked here.
# This is NOT a copy. It compiles the actual kernel test file from the repo root.
# If it passes here, the libstdio_bus.a that the C++ SDK links against is correct.
# =============================================================================
# NOTE: This target is DEVELOPMENT ONLY. It is excluded from install/packaging.
# It proves the .a file the SDK links against passes all 134 kernel invariants.
set(KERNEL_TEST_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../../../tests/test_main.c")

if(EXISTS "${KERNEL_TEST_SOURCE}")
    add_executable(stdiobus_kernel_tests "${KERNEL_TEST_SOURCE}")
    
    target_include_directories(stdiobus_kernel_tests PRIVATE
        "${STDIO_BUS_INCLUDE_DIR}"
    )
    
    target_link_libraries(stdiobus_kernel_tests PRIVATE
        ${STDIO_BUS_LIB_PATH}
    )
    
    # Platform define (same as kernel Makefile)
    if(APPLE)
        target_compile_definitions(stdiobus_kernel_tests PRIVATE STDIO_BUS_USE_KQUEUE _DEFAULT_SOURCE)
    else()
        target_compile_definitions(stdiobus_kernel_tests PRIVATE STDIO_BUS_USE_EPOLL _DEFAULT_SOURCE)
    endif()
    
    # Compile as C (not C++)
    set_target_properties(stdiobus_kernel_tests PROPERTIES
        LINKER_LANGUAGE C
        EXCLUDE_FROM_ALL FALSE
    )
    
    # Register as a single CTest that runs all 134 kernel tests
    add_test(
        NAME "KernelTests/all"
        COMMAND stdiobus_kernel_tests
    )
    set_tests_properties("KernelTests/all" PROPERTIES
        LABELS "kernel"
        TIMEOUT 30
    )
    
    # EXCLUDE from install — this is dev-only, never shipped
    # (no install() call for this target)
endif()

# Conformance test executable
# Mirrors kernel e2e tests (tests/e2e/) through the C++ SDK API.
# Uses the same echo-worker.js as kernel tests.
add_executable(stdiobus_conformance
    test_conformance.cpp
)

target_link_libraries(stdiobus_conformance PRIVATE
    stdiobus
    GTest::gtest_main
)

gtest_discover_tests(stdiobus_conformance
    TEST_PREFIX "Conformance/"
    PROPERTIES
        LABELS "conformance"
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/../../.."
        ENVIRONMENT "STDIO_BUS_ROOT=${CMAKE_CURRENT_SOURCE_DIR}/../../.."
)
