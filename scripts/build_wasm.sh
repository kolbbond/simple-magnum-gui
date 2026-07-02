#!/bin/bash
# Build simple-magnum-gui for WebAssembly/Emscripten

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}Building simple-magnum-gui for WASM${NC}"

# Get script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_DIR"

# Activate Emscripten SDK.
# Resolution order: an already-activated SDK on PATH, then $EMSDK_PATH, then $EMSDK
# (the latter is exported by emsdk_env.sh). No path is hardcoded.
if command -v emcc >/dev/null 2>&1; then
    echo -e "${BLUE}Using emcc already on PATH${NC}"
elif [ -n "${EMSDK_PATH:-}" ] && [ -f "${EMSDK_PATH}/emsdk_env.sh" ]; then
    source "${EMSDK_PATH}/emsdk_env.sh"
elif [ -n "${EMSDK:-}" ] && [ -f "${EMSDK}/emsdk_env.sh" ]; then
    source "${EMSDK}/emsdk_env.sh"
else
    echo -e "${RED}Error: Emscripten SDK not found.${NC}"
    echo "Activate it first  (source /path/to/emsdk/emsdk_env.sh)"
    echo "or set EMSDK_PATH to your emsdk installation directory."
    exit 1
fi

# Build directories
BUILD_DIR="build-wasm"
NATIVE_BUILD_DIR="build-native"

# First, build native corrade-rc (needed for resource compilation during cross-compile)
CORRADE_RC="$PROJECT_DIR/$NATIVE_BUILD_DIR/Release/bin/corrade-rc"
if [ ! -f "$CORRADE_RC" ]; then
    echo -e "${BLUE}Building native corrade-rc tool...${NC}"
    cmake -B "$NATIVE_BUILD_DIR" -S external/corrade \
        -DCMAKE_BUILD_TYPE=Release \
        -DCORRADE_BUILD_TESTS=OFF \
        -DCORRADE_WITH_UTILITY=ON
    cmake --build "$NATIVE_BUILD_DIR" --target corrade-rc -j$(nproc)
fi

# Configure with emcmake
echo -e "${BLUE}Configuring CMake for Emscripten...${NC}"
emcmake cmake -B "$BUILD_DIR" -S . \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TESTING=OFF \
    -DENABLE_EXAMPLES=ON \
    -DENABLE_IMPLOT3D=ON \
    -DUSE_SYSTEM_MAGNUM=OFF \
    -DCORRADE_RC_EXECUTABLE="$CORRADE_RC"

# Build
echo -e "${BLUE}Building...${NC}"
cmake --build "$BUILD_DIR" -j$(nproc)

echo -e "${GREEN}Build complete!${NC}"
echo "Output files are in: $PROJECT_DIR/$BUILD_DIR"
