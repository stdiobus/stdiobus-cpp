#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# =============================================================================
# Sync prebuilt libstdio_bus.a from dist/lib/ into sdk/cpp/prebuilds/
# =============================================================================
#
# Run this after `make dist` or after updating the kernel.
# It copies the platform-specific .a files into the SDK package structure.
#
# Usage:
#   ./scripts/sync-prebuilds.sh
#
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
REPO_ROOT="$(cd "$SDK_DIR/../.." && pwd)"
DIST_LIB="$REPO_ROOT/dist/lib"

if [ ! -d "$DIST_LIB" ]; then
    echo "ERROR: dist/lib/ not found. Run 'make dist' first."
    exit 1
fi

echo "Syncing prebuilds from dist/lib/ → sdk/cpp/prebuilds/"
echo ""

PLATFORMS=(
    "aarch64-apple-darwin"
    "x86_64-apple-darwin"
    "aarch64-unknown-linux-gnu"
    "x86_64-unknown-linux-gnu"
)

for platform in "${PLATFORMS[@]}"; do
    src="$DIST_LIB/$platform/libstdio_bus.a"
    dst_dir="$SDK_DIR/prebuilds/$platform"
    
    if [ -f "$src" ]; then
        mkdir -p "$dst_dir"
        cp "$src" "$dst_dir/libstdio_bus.a"
        size=$(ls -lh "$dst_dir/libstdio_bus.a" | awk '{print $5}')
        echo "  ✓ $platform ($size)"
    else
        echo "  ○ $platform (not found in dist/lib/, skipping)"
    fi
done

# Also copy the kernel header needed by the SDK
HEADER_SRC="$REPO_ROOT/include/stdio_bus_embed.h"
HEADER_DST="$SDK_DIR/prebuilds/include/stdio_bus_embed.h"
if [ -f "$HEADER_SRC" ]; then
    mkdir -p "$(dirname "$HEADER_DST")"
    cp "$HEADER_SRC" "$HEADER_DST"
    echo "  ✓ include/stdio_bus_embed.h"
fi

echo ""
echo "Done. Prebuilds ready for packaging."
