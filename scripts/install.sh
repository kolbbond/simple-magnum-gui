#!/bin/bash
# Installation script for simple-magnum-gui
# This script installs dependencies and smg to a specified prefix

set -e

# Default install prefix
PREFIX="${1:-$HOME/.local}"

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"
BUILD_DIR="${PROJECT_DIR}/build_install"

echo "=== SMG Installation Script ==="
echo "Install prefix: $PREFIX"
echo "Project dir: $PROJECT_DIR"
echo "Build dir: $BUILD_DIR"
echo ""

# Create build directory
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Step 1: Build and install Corrade
echo ""
echo "=== Step 1: Installing Corrade ==="
mkdir -p corrade && cd corrade
cmake "$PROJECT_DIR/external/corrade" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCORRADE_BUILD_TESTS=OFF \
    -DCORRADE_WITH_TESTSUITE=OFF
cmake --build . --parallel "$(nproc)"
cmake --install .
cd ..

# Step 2: Build and install Magnum
echo ""
echo "=== Step 2: Installing Magnum ==="
mkdir -p magnum && cd magnum
cmake "$PROJECT_DIR/external/magnum" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DMAGNUM_WITH_SDL2APPLICATION=ON \
    -DMAGNUM_WITH_GL=ON \
    -DMAGNUM_WITH_MESHTOOLS=ON \
    -DMAGNUM_WITH_PRIMITIVES=ON \
    -DMAGNUM_WITH_SHADERS=ON \
    -DMAGNUM_WITH_TRADE=ON \
    -DMAGNUM_WITH_TEXT=ON \
    -DMAGNUM_WITH_MAGNUMFONT=ON \
    -DMAGNUM_BUILD_TESTS=OFF
cmake --build . --parallel "$(nproc)"
cmake --install .
cd ..

# Step 3: Build and install Magnum Plugins
echo ""
echo "=== Step 3: Installing Magnum Plugins ==="
mkdir -p magnum-plugins && cd magnum-plugins
cmake "$PROJECT_DIR/external/magnum-plugins" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DMAGNUM_WITH_ANYIMAGEIMPORTER=ON \
    -DMAGNUM_WITH_STBIMAGEIMPORTER=ON \
    -DMAGNUM_BUILD_TESTS=OFF
cmake --build . --parallel "$(nproc)"
cmake --install .
cd ..

# Step 4: Build and install Magnum Integration (ImGui)
echo ""
echo "=== Step 4: Installing Magnum Integration ==="
mkdir -p magnum-integration && cd magnum-integration
cmake "$PROJECT_DIR/external/magnum-integration" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DMAGNUM_WITH_IMGUI=ON \
    -DMAGNUM_BUILD_TESTS=OFF
cmake --build . --parallel "$(nproc)"
cmake --install .
cd ..

# Step 5: Build and install SMG using system packages
echo ""
echo "=== Step 5: Installing SMG ==="
mkdir -p smg && cd smg
cmake "$PROJECT_DIR" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_PREFIX_PATH="$PREFIX" \
    -DUSE_SYSTEM_MAGNUM=ON \
    -DCREATE_INSTALL=ON \
    -DENABLE_TESTING=OFF \
    -DENABLE_EXAMPLES=OFF
cmake --build . --parallel "$(nproc)"
cmake --install .
cd ..

echo ""
echo "=== Installation Complete ==="
echo "SMG and dependencies installed to: $PREFIX"
echo ""
echo "To use in your project, set CMAKE_PREFIX_PATH:"
echo "  cmake -DCMAKE_PREFIX_PATH=$PREFIX .."
echo ""
echo "CMakeLists.txt example:"
echo "  find_package(smg REQUIRED)"
echo "  target_link_libraries(myapp PRIVATE smg::smg)"
