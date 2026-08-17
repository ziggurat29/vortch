#!/usr/bin/env bash
# Usage: bash scripts/build.sh [preset]   (default: core)
# Requires VCPKG_ROOT to point at your vcpkg checkout.
set -euo pipefail

PRESET="${1:-core}"
: "${VCPKG_ROOT:?set VCPKG_ROOT to your vcpkg checkout, e.g. export VCPKG_ROOT=\$HOME/vcpkg}"

echo "=== configure ($PRESET) ==="
cmake --preset "$PRESET"
echo "=== build ($PRESET) ==="
cmake --build --preset "$PRESET"
echo "=== test ($PRESET) ==="
ctest --test-dir "build/$PRESET" --output-on-failure
