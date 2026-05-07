#!/bin/bash

#
# @license
# Copyright 2026-present Raman Marozau, raman@stdiobus.com
# SPDX-License-Identifier: Apache-2.0
#

# Release script for stdio Bus C++ SDK
# Usage: ./scripts/release.sh <version>
# Example: ./scripts/release.sh 1.1.0
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDK_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION="$1"

if [ -z "$VERSION" ]; then
    echo "Usage: $0 <version>"
    echo "Example: $0 1.1.0"
    exit 1
fi

# Validate version format
if ! echo "$VERSION" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "Error: Version must be in format X.Y.Z (e.g., 1.0.0)"
    exit 1
fi

echo "=== Releasing stdio Bus C++ SDK v$VERSION ==="
echo ""

# Step 1: Verify clean working tree
if [ -n "$(git status --porcelain)" ]; then
    echo "Error: Working tree is not clean. Commit or stash changes first."
    exit 1
fi

# Step 2: Run full verification
echo "--- Running full verification ---"
"$SCRIPT_DIR/verify.sh"
echo ""

# Step 3: Update version in source files
echo "--- Updating version to $VERSION ---"

MAJOR=$(echo "$VERSION" | cut -d. -f1)
MINOR=$(echo "$VERSION" | cut -d. -f2)
PATCH=$(echo "$VERSION" | cut -d. -f3)
VERSION_NUMBER=$((MAJOR * 10000 + MINOR * 100 + PATCH))

# Update version.hpp
sed -i.bak \
    -e "s/#define STDIOBUS_VERSION_MAJOR .*/#define STDIOBUS_VERSION_MAJOR $MAJOR/" \
    -e "s/#define STDIOBUS_VERSION_MINOR .*/#define STDIOBUS_VERSION_MINOR $MINOR/" \
    -e "s/#define STDIOBUS_VERSION_PATCH .*/#define STDIOBUS_VERSION_PATCH $PATCH/" \
    -e "s/#define STDIOBUS_VERSION_STRING .*/#define STDIOBUS_VERSION_STRING \"$VERSION\"/" \
    -e "s/#define STDIOBUS_VERSION_NUMBER .*/#define STDIOBUS_VERSION_NUMBER $VERSION_NUMBER/" \
    "$SDK_DIR/include/stdiobus/version.hpp"
rm -f "$SDK_DIR/include/stdiobus/version.hpp.bak"

# Update CMakeLists.txt
sed -i.bak "s/VERSION [0-9]*\.[0-9]*\.[0-9]*/VERSION $VERSION/" "$SDK_DIR/CMakeLists.txt"
rm -f "$SDK_DIR/CMakeLists.txt.bak"

# Update conanfile.py
sed -i.bak "s/version = \"[0-9]*\.[0-9]*\.[0-9]*\"/version = \"$VERSION\"/" "$SDK_DIR/conanfile.py"
rm -f "$SDK_DIR/conanfile.py.bak"

echo "  Updated version.hpp, CMakeLists.txt, conanfile.py"

# Step 4: Verify build with new version
echo ""
echo "--- Verifying build with new version ---"
"$SCRIPT_DIR/build.sh" --release --no-tests >/dev/null 2>&1
echo "  ✓ Build successful"

# Step 5: Commit and tag
echo ""
echo "--- Creating release commit and tag ---"
git add -A
git commit -m "release: v$VERSION"
git tag -a "v$VERSION" -m "Release v$VERSION"

echo ""
echo "=== Release v$VERSION prepared ==="
echo ""
echo "Next steps:"
echo "  1. Review: git log --oneline -3"
echo "  2. Push:   git push origin main --tags"
echo "  3. GitHub Actions will create the release automatically"
