#!/bin/bash
set -e

# Get the location of the godot scene from the command line argument
# and report an error if not provided

if [ -z "$1" ]; then
    echo "Error: No Godot scene directory provided."
    echo "Usage: $0 <path_to_godot_scene_dir>"
    exit 1
fi

GODOT_SCENE_DIR="$1"

# the structure of the Godot scene should be
# <path_to_godot_scene>/project.godot
# So check that it exists, and error out if not

if [ ! -f "${GODOT_SCENE_DIR}/project.godot" ]; then
    echo "Error: project.godot not found in the specified path: ${GODOT_SCENE_DIR}"
    exit 1
fi

# mkdir addons in the GODOT_SCENE_DIR with -p
mkdir -p "${GODOT_SCENE_DIR}/addons"

# Recursively copy the contents of addons to the addons directory in the GODOT_SCENE_DIR
cp -r addons/* "${GODOT_SCENE_DIR}/addons/"

echo "Successfully copied the plugin to ${GODOT_SCENE_DIR}/addons/"

mkdir -p "${GODOT_SCENE_DIR}/lib"

# Recursively copy build-usd/install/plugin to lib/plugin
cp -r build-usd/install/plugin "${GODOT_SCENE_DIR}/lib/"
cp -r build-usd/install/lib/usd "${GODOT_SCENE_DIR}/lib/"

# This function copies the specified file to the destination if it doesn't exist
copy_if_not_exists() {
    local src_file="$1"
    local dest_file="$2"

    if [ ! -f "$dest_file" ]; then
        cp "$src_file" "$dest_file"
    fi

    # Symlinking to expected .dylib names for macOS dynamic linker compatibility
    # we need to symlink in the destination directory lib.x.y.z.dylib to lib.x.y.dylib
    local dest_file_base=$(basename "$dest_file")
    local dest_file_base_no_ext="${dest_file_base%.*}"
    local dest_file_base_no_ext_no_version="${dest_file_base_no_ext%.*}"

    ln -sf "$dest_file" "${GODOT_SCENE_DIR}/lib/${dest_file_base_no_ext_no_version}.dylib"
}

copy_if_not_exists "build-usd/deps/install/lib/libosdCPU.3.6.0.dylib" "${GODOT_SCENE_DIR}/lib/libosdCPU.3.6.0.dylib"
copy_if_not_exists "build-usd/deps/install/lib/libosdGPU.3.6.0.dylib" "${GODOT_SCENE_DIR}/lib/libosdGPU.3.6.0.dylib"

copy_if_not_exists "build-usd/deps/install/lib/libtbb.12.15.dylib" "${GODOT_SCENE_DIR}/lib/libtbb.12.15.dylib"
copy_if_not_exists "build-usd/deps/install/lib/libtbbmalloc.2.15.dylib" "${GODOT_SCENE_DIR}/lib/libtbbmalloc.2.15.dylib"
copy_if_not_exists "build-usd/deps/install/lib/libtbbmalloc_proxy.2.15.dylib" "${GODOT_SCENE_DIR}/lib/libtbbmalloc_proxy.2.15.dylib"

echo "Successfully copied the dylibs to ${GODOT_SCENE_DIR}/lib/"

plugin="libgodot-usd.dylib"
plugin_path="bin/${plugin}"

# Copy the plugin from bin to lib
cp "${plugin_path}" "${GODOT_SCENE_DIR}/lib/"

echo "Successfully copied the plugin to ${GODOT_SCENE_DIR}/lib/"
echo "Build and installation completed successfully!"
echo "You can now run your Godot project with the USD plugin."
echo "To use the plugin, make sure to add the 'addons' directory to your Godot project."
