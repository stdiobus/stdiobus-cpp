#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# Build script for stdio Bus C++ SDK
# Usage: ./scripts/build.sh [--release] [--no-tests] [--no-examples]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

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

echo "=== Building stdio Bus C++ SDK ==="
echo "  Build type: $BUILD_TYPE"
echo "  Tests: $BUILD_TESTS"
echo "  Examples: $BUILD_EXAMPLES"
echo ""

# Configure CMake (uses prebuilds/ automatically via CMakeLists.txt)
echo "--- Configuring CMake ---"
BUILD_DIR="$SDK_DIR/build"

CMAKE_ARGS=(
    -S "$SDK_DIR"
    -B "$BUILD_DIR"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DSTDIOBUS_BUILD_TESTS="$BUILD_TESTS"
    -DSTDIOBUS_BUILD_EXAMPLES="$BUILD_EXAMPLES"
)

cmake "${CMAKE_ARGS[@]}"
echo ""

# Build
echo "--- Building ---"
cmake --build "$BUILD_DIR" --parallel
echo ""

echo "=== Build complete ==="
echo "  Library: $BUILD_DIR/libstdiobus.a"
if [ "$BUILD_TESTS" = "ON" ]; then
    echo "  Tests:   $BUILD_DIR/tests/stdiobus_tests"
fi
if [ "$BUILD_EXAMPLES" = "ON" ]; then
    echo "  Examples: $BUILD_DIR/examples/"
fi
