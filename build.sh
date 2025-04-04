#!/bin/bash
set -e

# Check if USD_INSTALL_DIR is defined
if [ -z "$USD_INSTALL_DIR" ]; then
    echo "Error: USD_INSTALL_DIR environment variable is not set."
    echo "Please set it to the path where USD is installed."
    echo "Example: export USD_INSTALL_DIR=/path/to/usd/install"
    exit 1
fi

# Check if GODOT_SOURCE_DIR is defined
if [ -z "$GODOT_SOURCE_DIR" ]; then
    echo "Error: GODOT_SOURCE_DIR environment variable is not set."
    echo "Please set it to the path where Godot engine source code is located."
    echo "Example: export GODOT_SOURCE_DIR=/path/to/godot/source"
    exit 1
fi

# Verify that GODOT_SOURCE_DIR contains the required files
if [ ! -f "${GODOT_SOURCE_DIR}/core/extension/gdextension_interface.h" ]; then
    echo "Error: gdextension_interface.h not found in Godot source at ${GODOT_SOURCE_DIR}/core/extension/gdextension_interface.h"
    echo "Please make sure GODOT_SOURCE_DIR points to a valid Godot engine source directory."
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. \
  -DUSD_INSTALL_DIR=${USD_INSTALL_DIR} \
  -DGODOT_SOURCE_DIR=${GODOT_SOURCE_DIR} \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build
if [ "$(uname)" == "Darwin" ]; then
    # macOS
    cmake --build . -j$(sysctl -n hw.ncpu)
else
    # Linux and others
    cmake --build . -j$(nproc)
fi

# Install to local addons directory
cmake --install . --prefix=../

echo "Build completed successfully!"
echo "The plugin has been installed to the 'addons/godot-usd' directory."
echo "To use the plugin, copy the 'addons' directory to your Godot project."
