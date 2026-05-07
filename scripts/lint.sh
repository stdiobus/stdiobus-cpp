#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# Run static analysis on C++ source files
# Usage: ./scripts/lint.sh [--fix]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
BUILD_DIR="$SDK_DIR/build"

FIX=""
for arg in "$@"; do
    case "$arg" in
        --fix) FIX="--fix" ;;
        --help|-h)
            echo "Usage: $0 [--fix]"
            echo ""
            echo "  --fix   Apply suggested fixes automatically"
            exit 0
            ;;
    esac
done

# Check for clang-tidy
CLANG_TIDY=$(command -v clang-tidy 2>/dev/null || true)
if [ -z "$CLANG_TIDY" ]; then
    echo "Error: clang-tidy not found. Install with:"
    echo "  brew install llvm           # macOS"
    echo "  apt install clang-tidy      # Ubuntu"
    exit 1
fi

echo "Using: $($CLANG_TIDY --version | head -1)"

# Ensure compile_commands.json exists
if [ ! -f "$BUILD_DIR/compile_commands.json" ]; then
    echo "compile_commands.json not found. Building with export..."
    cmake -S "$SDK_DIR" -B "$BUILD_DIR" -DCMAKE_EXPORT_COMPILE_COMMANDS=ON >/dev/null 2>&1
fi

# Source files to lint
FILES=$(find "$SDK_DIR/src" "$SDK_DIR/include" \
    -name "*.cpp" -o -name "*.hpp" 2>/dev/null | sort)

echo "Running clang-tidy on source files..."
echo ""

FAILED=0
for f in $FILES; do
    if ! $CLANG_TIDY $FIX -p "$BUILD_DIR" "$f" 2>/dev/null; then
        FAILED=$((FAILED + 1))
    fi
done

echo ""
if [ $FAILED -gt 0 ]; then
    echo "✗ clang-tidy found issues in $FAILED file(s)"
    exit 1
else
    echo "✓ clang-tidy passed"
fi
