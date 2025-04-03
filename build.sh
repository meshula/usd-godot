#!/bin/bash
set -e

# Check if USD_INSTALL_DIR is defined
if [ -z "$USD_INSTALL_DIR" ]; then
    echo "Error: USD_INSTALL_DIR environment variable is not set."
    echo "Please set it to the path where USD is installed."
    echo "Example: export USD_INSTALL_DIR=/path/to/usd/install"
    exit 1
fi

# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. \
  -DUSD_INSTALL_DIR=${USD_INSTALL_DIR} \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo

# Build
cmake --build . -j$(nproc)

# Install to local addons directory
cmake --install . --prefix=../

echo "Build completed successfully!"
echo "The plugin has been installed to the 'addons/godot-usd' directory."
echo "To use the plugin, copy the 'addons' directory to your Godot project."
