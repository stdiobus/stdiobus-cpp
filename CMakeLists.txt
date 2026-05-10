cmake_minimum_required(VERSION 3.14)
project(stdiobus-cpp 
    VERSION 1.0.0 
    DESCRIPTION "C++ SDK for stdio Bus - AI agent transport layer"
    HOMEPAGE_URL "https://stdiobus.com"
    LANGUAGES C CXX
)

# =============================================================================
# Language Standard
# =============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# =============================================================================
# CMake Modules
# =============================================================================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(cmake/ProjectOptions.cmake)
include(cmake/Warnings.cmake)
include(cmake/Sanitizers.cmake)

# =============================================================================
# macOS SDK detection
# =============================================================================
if(APPLE AND NOT CMAKE_OSX_SYSROOT)
    execute_process(
        COMMAND xcrun --show-sdk-path
        OUTPUT_VARIABLE _sdk_path
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(_sdk_path)
        set(CMAKE_OSX_SYSROOT "${_sdk_path}")
        # Workaround: some CLT versions do not search <sysroot>/usr/include/c++/v1
        if(EXISTS "${_sdk_path}/usr/include/c++/v1")
            include_directories(SYSTEM "${_sdk_path}/usr/include/c++/v1")
        endif()
    endif()
endif()

# =============================================================================
# Package Metadata
# =============================================================================
set(STDIOBUS_AUTHOR "stdiobus contributors")
set(STDIOBUS_LICENSE "Apache-2.0")

# =============================================================================
# Find libstdio_bus (prebuilt C kernel)
# =============================================================================
if(DEFINED STDIO_BUS_LIB_DIR)
    set(STDIO_BUS_LIB_PATH "${STDIO_BUS_LIB_DIR}/libstdio_bus.a")
    set(STDIO_BUS_INCLUDE_DIR "${STDIO_BUS_LIB_DIR}/../include")
else()
    # Auto-detect from prebuilds based on platform
    if(APPLE)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
            set(_PREBUILD_TRIPLE "aarch64-apple-darwin")
        else()
            set(_PREBUILD_TRIPLE "x86_64-apple-darwin")
        endif()
    elseif(UNIX)
        if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64")
            set(_PREBUILD_TRIPLE "aarch64-unknown-linux-gnu")
        else()
            set(_PREBUILD_TRIPLE "x86_64-unknown-linux-gnu")
        endif()
    endif()

    if(DEFINED _PREBUILD_TRIPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/prebuilds/${_PREBUILD_TRIPLE}/libstdio_bus.a")
        set(STDIO_BUS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/prebuilds/${_PREBUILD_TRIPLE}/libstdio_bus.a")
        set(STDIO_BUS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/prebuilds/include")
        message(STATUS "Using prebuilt kernel: ${_PREBUILD_TRIPLE}")
    else()
        set(STDIO_BUS_LIB_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../build/libstdio_bus.a")
        set(STDIO_BUS_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../include")
    endif()
endif()

# =============================================================================
# STDIOBUS_ENABLE_C_KERNEL Option (auto-detect based on libstdio_bus.a presence)
# =============================================================================
if(EXISTS "${STDIO_BUS_LIB_PATH}")
    option(STDIOBUS_ENABLE_C_KERNEL "Enable C kernel adapter (requires libstdio_bus.a)" ON)
else()
    option(STDIOBUS_ENABLE_C_KERNEL "Enable C kernel adapter (requires libstdio_bus.a)" OFF)
endif()

if(STDIOBUS_ENABLE_C_KERNEL AND NOT EXISTS "${STDIO_BUS_LIB_PATH}")
    message(WARNING "STDIOBUS_ENABLE_C_KERNEL=ON but libstdio_bus.a not found at: ${STDIO_BUS_LIB_PATH}")
    message(WARNING "Build the kernel first or set STDIO_BUS_LIB_DIR")
endif()

# =============================================================================
# Library Targets
# =============================================================================

# Header-only interface library (public headers + kernel header)
add_library(stdiobus-headers INTERFACE)
add_library(stdiobus::headers ALIAS stdiobus-headers)
target_include_directories(stdiobus-headers INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${STDIO_BUS_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(STDIOBUS_CPP_EXCEPTIONS)
    target_compile_definitions(stdiobus-headers INTERFACE STDIOBUS_CPP_EXCEPTIONS=1)
endif()

# Imported target for prebuilt C kernel (not exported — avoids absolute paths in install)
if(STDIOBUS_ENABLE_C_KERNEL AND EXISTS "${STDIO_BUS_LIB_PATH}")
    add_library(stdio_bus_kernel STATIC IMPORTED)
    set_target_properties(stdio_bus_kernel PROPERTIES
        IMPORTED_LOCATION "${STDIO_BUS_LIB_PATH}"
    )
endif()

# Core sources (always compiled)
set(STDIOBUS_SOURCES
    src/bus.cpp
    src/echo_kernel.cpp
)

# Conditional C kernel adapter
if(STDIOBUS_ENABLE_C_KERNEL)
    list(APPEND STDIOBUS_SOURCES src/c_kernel.cpp)
endif()

# Static library with implementation
add_library(stdiobus STATIC ${STDIOBUS_SOURCES})
add_library(stdiobus::stdiobus ALIAS stdiobus)
target_link_libraries(stdiobus PUBLIC stdiobus-headers)

if(STDIOBUS_ENABLE_C_KERNEL)
    # Define STDIOBUS_HAS_C_KERNEL when C kernel is enabled
    target_compile_definitions(stdiobus PUBLIC STDIOBUS_HAS_C_KERNEL=1)
    # Link privately during build (absolute path, not exported)
    target_link_libraries(stdiobus PRIVATE $<BUILD_INTERFACE:stdio_bus_kernel>)
    # For installed consumers: link by library name (found in same lib/ dir)
    target_link_libraries(stdiobus INTERFACE $<INSTALL_INTERFACE:stdio_bus>)
    message(STATUS "C kernel adapter: ENABLED")
else()
    message(STATUS "C kernel adapter: DISABLED (pure C++ mode)")
endif()

# Apply warnings to our own code only (PRIVATE)
stdiobus_set_warnings(stdiobus)

# Platform-specific link dependencies
if(UNIX AND NOT APPLE)
    target_link_libraries(stdiobus PUBLIC pthread)
endif()

# =============================================================================
# Tests
# =============================================================================
if(STDIOBUS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# =============================================================================
# Examples
# =============================================================================
if(STDIOBUS_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# =============================================================================
# Benchmarks
# =============================================================================
if(STDIOBUS_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

# =============================================================================
# Fuzz Tests
# =============================================================================
if(STDIOBUS_BUILD_FUZZ)
    add_subdirectory(tests/fuzz)
endif()

# =============================================================================
# Install
# =============================================================================
if(STDIOBUS_INSTALL)
    include(GNUInstallDirs)

    install(TARGETS stdiobus stdiobus-headers
        EXPORT stdiobus-targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Public headers
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    # Kernel C header (required by ffi.hpp)
    if(EXISTS "${STDIO_BUS_INCLUDE_DIR}/stdio_bus_embed.h")
        install(FILES "${STDIO_BUS_INCLUDE_DIR}/stdio_bus_embed.h"
            DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    endif()

    # Prebuilt kernel library
    if(EXISTS "${STDIO_BUS_LIB_PATH}")
        install(FILES "${STDIO_BUS_LIB_PATH}"
            DESTINATION ${CMAKE_INSTALL_LIBDIR})
    endif()

    # License and docs
    install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/stdiobus)
    install(FILES README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

    # CMake package config
    install(EXPORT stdiobus-targets
        FILE stdiobus-targets.cmake
        NAMESPACE stdiobus::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stdiobus
    )

    # Config file for find_package()
    include(CMakePackageConfigHelpers)

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/stdiobus-config.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/stdiobus-config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stdiobus
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/stdiobus-config-version.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/stdiobus-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/stdiobus-config-version.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/stdiobus
    )

    # pkg-config
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/stdiobus.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/stdiobus.pc"
        @ONLY
    )
    install(FILES "${CMAKE_CURRENT_BINARY_DIR}/stdiobus.pc"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig
    )
endif()
