#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# Build script for stdiobus C++ SDK
# Usage: ./scripts/build.sh [--release] [--no-tests] [--no-examples]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$SDK_DIR/../.." && pwd)"

BUILD_TYPE="Debug"
BUILD_TESTS="ON"
BUILD_EXAMPLES="ON"

for arg in "$@"; do
    case "$arg" in
        --release) BUILD_TYPE="Release" ;;
        --no-tests) BUILD_TESTS="OFF" ;;
        --no-examples) BUILD_EXAMPLES="OFF" ;;
        --help|-h)
            echo "Usage: $0 [--release] [--no-tests] [--no-examples]"
            exit 0
            ;;
    esac
done

echo "=== Building stdiobus C++ SDK ==="
echo "  Build type: $BUILD_TYPE"
echo "  Tests: $BUILD_TESTS"
echo "  Examples: $BUILD_EXAMPLES"
echo ""

# Step 1: Build the C kernel static library
echo "--- Building C kernel (libstdio_bus.a) ---"
make -C "$REPO_ROOT" lib
echo ""

# Step 2: Configure CMake
echo "--- Configuring CMake ---"
BUILD_DIR="$SDK_DIR/build"

CMAKE_ARGS=(
    -S "$SDK_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DSTDIO_BUS_LIB_DIR="$REPO_ROOT/build"
    -DSTDIOBUS_BUILD_TESTS="$BUILD_TESTS"
    -DSTDIOBUS_BUILD_EXAMPLES="$BUILD_EXAMPLES"
)

# macOS: help find C++ headers if needed
if [ "$(uname)" = "Darwin" ]; then
    SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || true)
    if [ -n "$SDK_PATH" ]; then
        CXX_INCLUDE="$SDK_PATH/usr/include/c++/v1"
        if [ -d "$CXX_INCLUDE" ]; then
            CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="-I$CXX_INCLUDE")
        fi
    fi
fi

cmake "${CMAKE_ARGS[@]}"
echo ""

# Step 3: Build
echo "--- Building ---"
cmake --build "$BUILD_DIR" --parallel
echo ""

echo "=== Build complete ==="
echo "  Library: $BUILD_DIR/libstdiobus.a"
if [ "$BUILD_TESTS" = "ON" ]; then
    echo "  Tests:   $BUILD_DIR/tests/stdiobus_tests"
    echo "  E2E:     $BUILD_DIR/tests/stdiobus_e2e"
fi
if [ "$BUILD_EXAMPLES" = "ON" ]; then
    echo "  Examples: $BUILD_DIR/examples/"
fi
