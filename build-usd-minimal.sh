#!/bin/bash
set -e

# Set up environment variables for paths, adjust as necessary
export USD_SOURCE_DIR=${PWD}/build-usd/OpenUSD               # USD source code location
export USD_DEPS_DIR=${PWD}/build-usd/deps                    # Dependencies directory
export USD_BUILD_DIR=${PWD}/build-usd/build                  # USD build directory
export USD_INSTALL_DIR=${PWD}/build-usd/install              # USD installation directory

# Create necessary directories
mkdir -p ${USD_DEPS_DIR}/install
mkdir -p ${USD_BUILD_DIR}
mkdir -p ${USD_SOURCE_DIR}

echo "=== Building USD and dependencies ==="
echo "USD_SOURCE_DIR: ${USD_SOURCE_DIR}"
echo "USD_DEPS_DIR: ${USD_DEPS_DIR}"
echo "USD_BUILD_DIR: ${USD_BUILD_DIR}"
echo "USD_INSTALL_DIR: ${USD_INSTALL_DIR}"
echo ""

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check for required tools
if ! command_exists cmake; then
    echo "Error: cmake is required but not installed. Please install cmake."
    exit 1
fi

if ! command_exists git; then
    echo "Error: git is required but not installed. Please install git."
    exit 1
fi

if ! command_exists curl; then
    echo "Error: curl is required but not installed. Please install curl."
    exit 1
fi

if ! command_exists unzip; then
    echo "Error: unzip is required but not installed. Please install unzip."
    exit 1
fi

echo "=== Building TBB (Threading Building Blocks) ==="
cd ${USD_DEPS_DIR}

TBB_TAG="2022.1.0"
TBB_ZIP="oneTBB-v${TBB_TAG}.zip"
TBB_DIR="oneTBB-${TBB_TAG}"
TARGET_DIR="oneTBB"

# Download and extract TBB if not already done
if [ ! -d "$TARGET_DIR" ]; then
    echo "Downloading TBB version ${TBB_TAG}..."
    curl -L "https://github.com/oneapi-src/oneTBB/archive/refs/tags/v${TBB_TAG}.zip" --output "$TBB_ZIP"
    unzip "$TBB_ZIP"
    mv "$TBB_DIR" "$TARGET_DIR"
fi

# Build TBB if not already built
if [ ! -f "${USD_DEPS_DIR}/install/lib/libtbb.a" ] && [ ! -f "${USD_DEPS_DIR}/install/lib/libtbb.dylib" ] && [ ! -f "${USD_DEPS_DIR}/install/lib/libtbb.so" ]; then
    echo "Building TBB..."
    cd "$TARGET_DIR" && mkdir -p build && cd build
    cmake .. -DTBB_TEST=OFF -DTBB_STRICT=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${USD_DEPS_DIR}/install
    cmake --build . --config Release && cmake --install .
    cd ${USD_DEPS_DIR}
else
    echo "TBB already built, skipping..."
fi

echo "=== Building OpenSubdiv (Minimal Build) ==="
cd ${USD_DEPS_DIR}

# Download and extract OpenSubdiv if not already done
if [ ! -d "OpenSubdiv" ]; then
    echo "Downloading OpenSubdiv..."
    curl -L https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_6_0.zip --output OpenSubdiv.3.6.0.zip
    unzip OpenSubdiv.3.6.0.zip && mv OpenSubdiv-3_6_0/ OpenSubdiv
fi

# Build OpenSubdiv if not already built
if [ ! -f "${USD_DEPS_DIR}/install/lib/libosdCPU.a" ] && [ ! -f "${USD_DEPS_DIR}/install/lib/libosdCPU.dylib" ] && [ ! -f "${USD_DEPS_DIR}/install/lib/libosdCPU.so" ]; then
    echo "Building OpenSubdiv..."
    cd OpenSubdiv && mkdir -p build && cd build
    cmake .. -DNO_OPENGL=ON -DNO_EXAMPLES=ON -DNO_TUTORIALS=ON \
      -DNO_REGRESSION=ON -DNO_DOC=ON -DNO_OMP=ON -DNO_CUDA=ON \
      -DNO_OPENCL=ON -DNO_DX=ON -DNO_TESTS=ON -DNO_GLEW=ON \
      -DNO_GLFW=ON -DNO_PTEX=ON -DNO_TBB=ON \
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${USD_DEPS_DIR}/install
    cmake --build . --config Release && cmake --install .
    cd ${USD_DEPS_DIR}
else
    echo "OpenSubdiv already built, skipping..."
fi

echo "=== Getting USD Source ==="
# Clone the USD repository if not already done
if [ ! -d "${USD_SOURCE_DIR}/.git" ]; then
    echo "Cloning USD repository..."
    git clone https://github.com/PixarAnimationStudios/USD.git ${USD_SOURCE_DIR}
    cd ${USD_SOURCE_DIR}
    # Check out a specific version
    git checkout dev
else
    echo "USD repository already cloned, skipping..."
fi

echo "=== Building USD ==="
cd ${USD_BUILD_DIR}

# Configure USD with minimal features
echo "Configuring USD..."
cmake ${USD_SOURCE_DIR} \
  -DCMAKE_INSTALL_PREFIX=${USD_INSTALL_DIR} \
  -DPXR_ENABLE_PYTHON_SUPPORT=OFF \
  -DPXR_BUILD_TESTS=OFF \
  -DPXR_BUILD_EXAMPLES=OFF \
  -DPXR_BUILD_TUTORIALS=OFF \
  -DPXR_BUILD_USD_TOOLS=OFF \
  -DPXR_BUILD_IMAGING=ON \
  -DPXR_BUILD_USD_IMAGING=ON \
  -DPXR_BUILD_USDVIEW=OFF \
  -DPXR_ENABLE_MATERIALX_SUPPORT=OFF \
  -DPXR_ENABLE_HDF5_SUPPORT=OFF \
  -DPXR_ENABLE_OPENVDB_SUPPORT=OFF \
  -DPXR_ENABLE_OSL_SUPPORT=OFF \
  -DPXR_ENABLE_PTEX_SUPPORT=OFF \
  -DPXR_ENABLE_NAMESPACES=ON \
  -DPXR_MONOLITHIC_INSTALL=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_PREFIX_PATH=${USD_DEPS_DIR}/install \
  -DTBB_ROOT=${USD_DEPS_DIR}/install \
  -DCMAKE_BUILD_TYPE=Release

# Build and install USD
echo "Building USD..."
# Determine number of CPU cores for parallel build
if [ "$(uname)" == "Darwin" ]; then
    # macOS
    NUM_CORES=$(sysctl -n hw.ncpu)
else
    # Linux and others
    NUM_CORES=$(nproc)
fi
cmake --build . --config Release -j${NUM_CORES} --target install

echo "=== USD Build Complete ==="
echo "USD has been built and installed to: ${USD_INSTALL_DIR}"
echo ""
echo "To build the Godot-USD plugin, set the following environment variables:"
echo "export USD_INSTALL_DIR=${USD_INSTALL_DIR}"
echo "export GODOT_SOURCE_DIR=/path/to/godot/source"
echo ""
echo "Then run: ./build.sh"
