#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# Format all C++ source files using clang-format
# Usage: ./scripts/format.sh [--check]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

CHECK_ONLY=false
for arg in "$@"; do
    case "$arg" in
        --check) CHECK_ONLY=true ;;
        --help|-h)
            echo "Usage: $0 [--check]"
            echo ""
            echo "  --check   Check formatting without modifying files (exit 1 if unformatted)"
            exit 0
            ;;
    esac
done

# Find clang-format
CLANG_FORMAT=$(command -v clang-format 2>/dev/null || \
               command -v clang-format-18 2>/dev/null || \
               command -v clang-format-17 2>/dev/null || \
               command -v clang-format-16 2>/dev/null || \
               command -v clang-format-15 2>/dev/null || true)
if [ -z "$CLANG_FORMAT" ]; then
    echo "Error: clang-format not found. Install with:"
    echo "  brew install clang-format    # macOS"
    echo "  apt install clang-format     # Ubuntu"
    exit 1
fi

echo "Using: $($CLANG_FORMAT --version)"

# Collect source files
FILES=$(find "$SDK_DIR/include" "$SDK_DIR/src" "$SDK_DIR/tests" "$SDK_DIR/examples" \
    -name "*.cpp" -o -name "*.hpp" -o -name "*.h" 2>/dev/null | sort)

if [ -z "$FILES" ]; then
    echo "No source files found."
    exit 0
fi

FILE_COUNT=$(echo "$FILES" | wc -l | tr -d ' ')

if [ "$CHECK_ONLY" = true ]; then
    echo "Checking formatting of $FILE_COUNT files..."
    UNFORMATTED=0
    for f in $FILES; do
        if ! $CLANG_FORMAT --dry-run --Werror "$f" 2>/dev/null; then
            echo "  ✗ $f"
            UNFORMATTED=$((UNFORMATTED + 1))
        fi
    done
    if [ $UNFORMATTED -gt 0 ]; then
        echo ""
        echo "$UNFORMATTED file(s) need formatting. Run: ./scripts/format.sh"
        exit 1
    fi
    echo "  ✓ All files formatted correctly"
else
    echo "Formatting $FILE_COUNT files..."
    for f in $FILES; do
        $CLANG_FORMAT -i "$f"
    done
    echo "  ✓ Done"
fi
