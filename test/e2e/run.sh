#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# =============================================================================
# E2E Test: Install SDK → Build external project → Run as real user
# =============================================================================
#
# This test simulates what a real C++ developer does:
#   1. cmake --install the SDK to a temp prefix
#   2. Create a separate project that uses find_package(stdiobus)
#   3. Compile it against the installed SDK
#   4. Run it and verify it works
#
# This catches:
#   - Missing headers in install
#   - Broken cmake config files
#   - Missing symbols in installed library
#   - API surface regressions
#
# Usage:
#   ./test/e2e/run.sh
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"
REPO_ROOT="$(cd "$SDK_DIR/../.." && pwd)"

echo "============================================"
echo "  stdio Bus C++ SDK — User E2E Test"
echo "============================================"
echo ""
echo "  Simulates: install → find_package → build → run"
echo ""

# ─── Step 1: Build the SDK ────────────────────────────────────────
echo "--- Step 1: Building SDK ---"
"$SDK_DIR/scripts/build.sh" --no-tests --no-examples >/dev/null 2>&1
echo "  ✓ SDK built"

# ─── Step 2: Install to temp prefix ──────────────────────────────
INSTALL_PREFIX=$(mktemp -d /tmp/stdiobus_install_XXXXXX)
echo ""
echo "--- Step 2: Installing to $INSTALL_PREFIX ---"

cmake --install "$SDK_DIR/build" --prefix "$INSTALL_PREFIX" 2>/dev/null
echo "  ✓ SDK installed"
echo "  Headers: $INSTALL_PREFIX/include/"
echo "  Library: $INSTALL_PREFIX/lib/"
echo "  CMake:   $INSTALL_PREFIX/lib/cmake/stdiobus/"

# ─── Step 3: Build external user project ─────────────────────────
USER_BUILD=$(mktemp -d /tmp/stdiobus_user_build_XXXXXX)
echo ""
echo "--- Step 3: Building user project (find_package) ---"

CMAKE_ARGS=(
    -S "$SCRIPT_DIR"
    -B "$USER_BUILD"
    -DCMAKE_PREFIX_PATH="$INSTALL_PREFIX"
)

# macOS: help find C++ headers
if [ "$(uname)" = "Darwin" ]; then
    SDK_PATH=$(xcrun --show-sdk-path 2>/dev/null || true)
    if [ -n "$SDK_PATH" ] && [ -d "$SDK_PATH/usr/include/c++/v1" ]; then
        CMAKE_ARGS+=(-DCMAKE_CXX_FLAGS="-I$SDK_PATH/usr/include/c++/v1")
    fi
fi

cmake "${CMAKE_ARGS[@]}" >/dev/null 2>&1
cmake --build "$USER_BUILD" 2>&1 | grep -v "^--" | grep -v "^$" || true
echo "  ✓ User project compiled with find_package(stdiobus)"

# ─── Step 4: Run the user app ────────────────────────────────────
echo ""
echo "--- Step 4: Running user application ---"
echo ""

if "$USER_BUILD/user_app" 2>&1; then
    EXIT_CODE=0
else
    EXIT_CODE=$?
fi

# ─── Cleanup ─────────────────────────────────────────────────────
rm -rf "$INSTALL_PREFIX" "$USER_BUILD"

echo ""
if [ $EXIT_CODE -eq 0 ]; then
    echo "============================================"
    echo "  ✓ USER E2E TEST PASSED"
    echo ""
    echo "  The SDK installs correctly and works as"
    echo "  an external dependency via find_package()."
    echo "============================================"
else
    echo "============================================"
    echo "  ✗ USER E2E TEST FAILED"
    echo "============================================"
fi

exit $EXIT_CODE
