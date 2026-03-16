#!/bin/bash
# Build a fully static Orion Proxy binary for Linux
# Run this inside Ubuntu WSL or any Linux environment
#
# Usage:   ./build-static-linux.sh
# Output:  dist/orion_proxy (static binary)

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$SCRIPT_DIR"

echo "=== Orion Proxy Static Linux Build ==="
echo ""

# Check for required tools
for tool in gcc cmake make; do
    if ! command -v $tool &>/dev/null; then
        echo "ERROR: $tool not found. Install with:"
        echo "  sudo apt-get install -y build-essential cmake"
        exit 1
    fi
done

# Check for static libc (needed for -static)
if [ ! -f /usr/lib/x86_64-linux-gnu/libc.a ] && [ ! -f /usr/lib/aarch64-linux-gnu/libc.a ]; then
    echo "Static libc not found. Installing..."
    sudo apt-get install -y libc6-dev
fi

# Check for static pthread
if [ ! -f /usr/lib/x86_64-linux-gnu/libpthread.a ] && [ ! -f /usr/lib/aarch64-linux-gnu/libpthread.a ]; then
    echo "Static pthread not found. Installing..."
    sudo apt-get install -y libc6-dev
fi

# Clean and build
BUILD_DIR="$SCRIPT_DIR/build-static"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

echo ""
echo "--- Running CMake (static build) ---"
cmake .. -DSTATIC_BUILD=ON -DCMAKE_BUILD_TYPE=Release

echo ""
echo "--- Compiling ---"
make -j$(nproc)

# Strip the binary to reduce size
strip orion_proxy 2>/dev/null || true

# Show result
echo ""
echo "--- Build Complete ---"
file orion_proxy
ls -lh orion_proxy
ldd orion_proxy 2>&1 || echo "(statically linked — no dynamic dependencies)"

# Copy to dist folder
DIST_DIR="$SCRIPT_DIR/dist"
mkdir -p "$DIST_DIR"
cp orion_proxy "$DIST_DIR/"

echo ""
echo "Binary ready at: $DIST_DIR/orion_proxy"
echo ""
