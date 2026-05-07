#!/bin/bash
#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#
# Full Verification Script for stdio Bus C++ SDK
# Usage: ./scripts/verify.sh [--skip-format]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SKIP_FORMAT=false
for arg in "$@"; do
    case "$arg" in
        --skip-format) SKIP_FORMAT=true ;;
        --help|-h)
            echo "Usage: $0 [--skip-format]"
            exit 0
            ;;
    esac
done

GREEN='\033[0;32m'
RED='\033[0;31m'
NC='\033[0m'
FAILED=0

echo "============================================================"
echo "  stdio Bus C++ SDK — Full Verification"
echo "============================================================"
echo ""

echo "--- Step 1: Build ---"
"$SCRIPT_DIR/build.sh"
echo -e "  ${GREEN}✓ Build successful${NC}"
echo ""

echo "--- Step 2: Unit Tests ---"
if ctest --test-dir "$SDK_DIR/build" --output-on-failure -E "^(E2E/|Conformance/)"; then
    echo -e "  ${GREEN}✓ Unit tests PASSED${NC}"
else
    echo -e "  ${RED}✗ Unit tests FAILED${NC}"
    FAILED=1
fi
echo ""

if [ "$SKIP_FORMAT" = false ]; then
    echo "--- Step 3: Format Check ---"
    if "$SCRIPT_DIR/format.sh" --check; then
        echo -e "  ${GREEN}✓ Format check PASSED${NC}"
    else
        echo -e "  ${RED}✗ Format check FAILED (run ./scripts/format.sh)${NC}"
        FAILED=1
    fi
    echo ""
fi

echo "============================================================"
if [ $FAILED -eq 0 ]; then
    echo -e "  ${GREEN}ALL VERIFICATION PASSED${NC}"
else
    echo -e "  ${RED}VERIFICATION FAILED${NC}"
    exit 1
fi
