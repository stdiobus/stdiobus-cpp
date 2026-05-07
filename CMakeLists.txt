cmake_minimum_required(VERSION 3.14)
project(stdiobus-cpp 
    VERSION 1.0.0 
    DESCRIPTION "C++ SDK for stdio_bus - AI agent transport layer"
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
        # Ensure C++ standard library headers are found
        set(_cxx_include "${_sdk_path}/usr/include/c++/v1")
        if(EXISTS "${_cxx_include}")
            add_compile_options(-isystem "${_cxx_include}")
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

if(NOT EXISTS "${STDIO_BUS_LIB_PATH}")
    message(WARNING "libstdio_bus.a not found at: ${STDIO_BUS_LIB_PATH}")
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

# Static library with implementation
add_library(stdiobus STATIC src/bus.cpp)
add_library(stdiobus::stdiobus ALIAS stdiobus)
target_link_libraries(stdiobus PUBLIC stdiobus-headers)
target_link_libraries(stdiobus PRIVATE ${STDIO_BUS_LIB_PATH})

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
