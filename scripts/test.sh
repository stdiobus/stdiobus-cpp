#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# Test script for stdio Bus C++ SDK
# Usage: ./scripts/test.sh [--unit] [--e2e] [--all] [--verbose]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SDK_DIR/build"

RUN_UNIT=false
RUN_E2E=false
RUN_CONFORMANCE=false
VERBOSE=""

for arg in "$@"; do
    case "$arg" in
        --unit) RUN_UNIT=true ;;
        --e2e) RUN_E2E=true ;;
        --conformance) RUN_CONFORMANCE=true ;;
        --all) RUN_UNIT=true; RUN_E2E=true; RUN_CONFORMANCE=true ;;
        --verbose|-v) VERBOSE="--output-on-failure" ;;
        --help|-h)
            echo "Usage: $0 [--unit] [--e2e] [--conformance] [--all] [--verbose]"
            echo ""
            echo "  --unit         Run unit tests only"
            echo "  --e2e          Run end-to-end tests only"
            echo "  --conformance  Run conformance tests (mirrors kernel e2e)"
            echo "  --all          Run all tests (default if no flag given)"
            echo "  --verbose      Show output on failure"
            exit 0
            ;;
    esac
done

# Default: run all
if [ "$RUN_UNIT" = false ] && [ "$RUN_E2E" = false ] && [ "$RUN_CONFORMANCE" = false ]; then
    RUN_UNIT=true
    RUN_E2E=true
    RUN_CONFORMANCE=true
fi

if [ ! -d "$BUILD_DIR" ]; then
    echo "Build directory not found. Run ./scripts/build.sh first."
    exit 1
fi

echo "=== Running stdio Bus C++ SDK tests ==="
echo ""

FAILED=0

if [ "$RUN_UNIT" = true ]; then
    echo "--- Unit Tests ---"
    if ctest --test-dir "$BUILD_DIR" $VERBOSE -E "^E2E/"; then
        echo "  ✓ Unit tests passed"
    else
        echo "  ✗ Unit tests FAILED"
        FAILED=1
    fi
    echo ""
fi

if [ "$RUN_E2E" = true ]; then
    echo "--- E2E Tests ---"
    if ctest --test-dir "$BUILD_DIR" $VERBOSE -L e2e -N 2>/dev/null | grep -q "Total Tests: 0"; then
        echo "  ⊘ E2E tests skipped (C kernel disabled — no real worker processes available)"
    elif ctest --test-dir "$BUILD_DIR" $VERBOSE -L e2e; then
        echo "  ✓ E2E tests passed"
    else
        echo "  ✗ E2E tests FAILED"
        FAILED=1
    fi
    echo ""
fi

if [ "$RUN_CONFORMANCE" = true ]; then
    echo "--- Conformance Tests (mirrors kernel e2e) ---"
    if ctest --test-dir "$BUILD_DIR" $VERBOSE -L conformance -N 2>/dev/null | grep -q "Total Tests: 0"; then
        echo "  ⊘ Conformance tests skipped (C kernel disabled — no real worker processes available)"
    elif ctest --test-dir "$BUILD_DIR" $VERBOSE -L conformance; then
        echo "  ✓ Conformance tests passed"
    else
        echo "  ✗ Conformance tests FAILED"
        FAILED=1
    fi
    echo ""
fi

if [ $FAILED -eq 0 ]; then
    echo "=== All tests passed ==="
else
    echo "=== SOME TESTS FAILED ==="
    exit 1
fi
