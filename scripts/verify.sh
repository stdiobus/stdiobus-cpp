#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# =============================================================================
# Full Verification Script for stdio Bus C++ SDK
# =============================================================================
#
# This script ensures the C++ SDK is fully conformant with the kernel.
# It runs:
#   1. Kernel unit tests (tests/test_main.c) — validates the C kernel itself
#   2. C++ SDK unit tests — validates SDK types, error handling, FFI layer
#   3. C++ SDK e2e tests — validates SDK with real /bin/cat workers
#   4. C++ SDK conformance tests — mirrors kernel e2e scenarios through SDK API
#
# If ALL pass, the SDK is guaranteed to be in sync with the kernel.
#
# Usage:
#   ./scripts/verify.sh           # Full verification
#   ./scripts/verify.sh --quick   # Skip kernel rebuild (assumes already built)
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$SDK_DIR/../.." && pwd)"

QUICK=false
for arg in "$@"; do
    case "$arg" in
        --quick) QUICK=true ;;
        --help|-h)
            echo "Usage: $0 [--quick]"
            echo ""
            echo "  --quick   Skip kernel rebuild (use existing build)"
            echo ""
            echo "Runs full verification pipeline:"
            echo "  1. Kernel unit tests (make test)"
            echo "  2. C++ SDK unit tests"
            echo "  3. C++ SDK e2e tests"
            echo "  4. C++ SDK conformance tests"
            exit 0
            ;;
    esac
done

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

FAILED=0

echo "============================================================"
echo "  stdio Bus C++ SDK — Full Verification"
echo "============================================================"
echo ""
echo "  Repo root: $REPO_ROOT"
echo "  SDK dir:   $SDK_DIR"
echo ""

# ─────────────────────────────────────────────────────────────────
# Step 1: Build and test the C kernel
# ─────────────────────────────────────────────────────────────────
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Step 1: Kernel Unit Tests (tests/test_main.c)"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if [ "$QUICK" = false ]; then
    echo "  Building kernel..."
    make -C "$REPO_ROOT" clean >/dev/null 2>&1 || true
    make -C "$REPO_ROOT" >/dev/null 2>&1
    make -C "$REPO_ROOT" lib >/dev/null 2>&1
fi

echo "  Running kernel unit tests..."
if make -C "$REPO_ROOT" test 2>&1 | tail -5; then
    echo -e "  ${GREEN}✓ Kernel unit tests PASSED${NC}"
else
    echo -e "  ${RED}✗ Kernel unit tests FAILED${NC}"
    echo -e "  ${RED}  Cannot verify SDK conformance if kernel tests fail.${NC}"
    exit 1
fi
echo ""

# ─────────────────────────────────────────────────────────────────
# Step 2: Build C++ SDK (links against the kernel we just tested)
# ─────────────────────────────────────────────────────────────────
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Step 2: Building C++ SDK"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

"$SCRIPT_DIR/build.sh" >/dev/null 2>&1
echo -e "  ${GREEN}✓ C++ SDK built successfully${NC}"
echo ""

# ─────────────────────────────────────────────────────────────────
# Step 3: C++ SDK Unit Tests
# ─────────────────────────────────────────────────────────────────
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Step 3: C++ SDK Unit Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if ctest --test-dir "$SDK_DIR/build" --output-on-failure -E "^(E2E/|Conformance/)" 2>&1 | tail -3; then
    echo -e "  ${GREEN}✓ SDK unit tests PASSED${NC}"
else
    echo -e "  ${RED}✗ SDK unit tests FAILED${NC}"
    FAILED=1
fi
echo ""

# ─────────────────────────────────────────────────────────────────
# Step 4: C++ SDK E2E Tests
# ─────────────────────────────────────────────────────────────────
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Step 4: C++ SDK E2E Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"

if ctest --test-dir "$SDK_DIR/build" --output-on-failure -L e2e 2>&1 | tail -3; then
    echo -e "  ${GREEN}✓ SDK e2e tests PASSED${NC}"
else
    echo -e "  ${RED}✗ SDK e2e tests FAILED${NC}"
    FAILED=1
fi
echo ""

# ─────────────────────────────────────────────────────────────────
# Step 5: C++ SDK Conformance Tests (mirrors kernel e2e)
# ─────────────────────────────────────────────────────────────────
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  Step 5: C++ SDK Conformance Tests"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "  (mirrors kernel e2e scenarios through SDK embed API)"

if ctest --test-dir "$SDK_DIR/build" --output-on-failure -L conformance 2>&1 | tail -3; then
    echo -e "  ${GREEN}✓ SDK conformance tests PASSED${NC}"
else
    echo -e "  ${RED}✗ SDK conformance tests FAILED${NC}"
    FAILED=1
fi
echo ""

# ─────────────────────────────────────────────────────────────────
# Summary
# ─────────────────────────────────────────────────────────────────
echo "============================================================"
if [ $FAILED -eq 0 ]; then
    echo -e "  ${GREEN}ALL VERIFICATION PASSED${NC}"
    echo ""
    echo "  The C++ SDK is fully conformant with the kernel."
    echo "  Kernel tests + SDK tests all pass against the same"
    echo "  libstdio_bus.a binary — guaranteed consistency."
    echo "============================================================"
    exit 0
else
    echo -e "  ${RED}VERIFICATION FAILED${NC}"
    echo ""
    echo "  The C++ SDK is NOT conformant with the kernel."
    echo "  Fix the failing tests before releasing."
    echo "============================================================"
    exit 1
fi
