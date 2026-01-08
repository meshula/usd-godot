#!/bin/bash
# CLI test runner for USD-Godot using GUT
#
# Usage:
#   ./run_tests.sh                    # Run all tests
#   ./run_tests.sh test_usd_document  # Run specific test file
#   ./run_tests.sh -v                 # Verbose output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# Find Godot executable
if [ -f "/Users/nporcino/dev/godot/bin/godot.macos.editor.arm64" ]; then
    GODOT_BIN="/Users/nporcino/dev/godot/bin/godot.macos.editor.arm64"
elif command -v godot &> /dev/null; then
    GODOT_BIN="godot"
elif [ -f "/Applications/Godot.app/Contents/MacOS/Godot" ]; then
    GODOT_BIN="/Applications/Godot.app/Contents/MacOS/Godot"
elif [ -f "$HOME/Applications/Godot.app/Contents/MacOS/Godot" ]; then
    GODOT_BIN="$HOME/Applications/Godot.app/Contents/MacOS/Godot"
else
    echo "Error: Godot executable not found."
    echo "Please install Godot 4.2+ or add it to your PATH."
    exit 1
fi

# Set USD plugin path for resolver
# USD's plug registry searches paths in PXR_PLUGINPATH_NAME for plugInfo.json files
# The lib/usd/plugInfo.json has "Includes": ["*/resources/"] which discovers ar, sdf, etc.
export PXR_PLUGINPATH_NAME="$SCRIPT_DIR/lib/usd"

echo "Using Godot: $GODOT_BIN"
echo "PXR_PLUGINPATH_NAME: $PXR_PLUGINPATH_NAME"
echo "Running tests..."
echo ""

# Build GUT command
GUT_ARGS="-d -s addons/gut/gut_cmdln.gd"

# Handle arguments
if [ "$1" == "-v" ] || [ "$1" == "--verbose" ]; then
    GUT_ARGS="$GUT_ARGS -glog=3"
    shift
elif [ -n "$1" ]; then
    # Run specific test file
    GUT_ARGS="$GUT_ARGS -gtest=res://tests/unit/$1.gd"
fi

# Run tests with headless Godot
$GODOT_BIN --headless $GUT_ARGS

echo ""
echo "Tests completed."
