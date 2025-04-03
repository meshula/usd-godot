# Building the Godot-USD Plugin

This guide provides instructions for building the Godot-USD integration plugin from source.

## Prerequisites

### Required Software

- **CMake** 3.26 or newer
- **C++17 compatible compiler**
  - GCC 9+ (Linux)
  - Clang 10+ (macOS)
  - MSVC 2019+ (Windows)
- **Godot Engine** 4.x source code
- **Git**

### Required Libraries

- **OpenUSD** - See [Building a Minimal USD](embeddingUSD.md) for instructions on building a minimal version of USD suitable for this plugin
- **TBB** (Thread Building Blocks) - For USD's threading system
- **OpenSubdiv** - For subdivision surface support in Hydra

## Environment Setup

Set up your environment variables to point to the required dependencies:

```bash
# Adjust these paths according to your system
export GODOT_SOURCE_DIR=/path/to/godot
export USD_INSTALL_DIR=/path/to/usd/install
export TBB_ROOT=/path/to/tbb/install
export OPENSUBDIV_ROOT=/path/to/opensubdiv/install
```

## Building on Linux/macOS

### 1. Clone the Repository

```bash
git clone https://github.com/your-organization/godot-usd.git
cd godot-usd
```

### 2. Create Build Directory

```bash
mkdir -p build
cd build
```

### 3. Configure with CMake

```bash
cmake .. \
  -DGODOT_SOURCE_DIR=${GODOT_SOURCE_DIR} \
  -DUSD_INSTALL_DIR=${USD_INSTALL_DIR} \
  -DTBB_ROOT=${TBB_ROOT} \
  -DOPENSUBDIV_ROOT=${OPENSUBDIV_ROOT} \
  -DCMAKE_BUILD_TYPE=Release
```

### 4. Build the Plugin

```bash
cmake --build . --config Release -j$(nproc)
```

### 5. Install the Plugin

```bash
cmake --install . --prefix=/path/to/your/godot/project/addons/usd
```

## Building on Windows

### 1. Clone the Repository

```batch
git clone https://github.com/your-organization/godot-usd.git
cd godot-usd
```

### 2. Create Build Directory

```batch
mkdir build
cd build
```

### 3. Configure with CMake

```batch
cmake .. ^
  -DGODOT_SOURCE_DIR=%GODOT_SOURCE_DIR% ^
  -DUSD_INSTALL_DIR=%USD_INSTALL_DIR% ^
  -DTBB_ROOT=%TBB_ROOT% ^
  -DOPENSUBDIV_ROOT=%OPENSUBDIV_ROOT% ^
  -DCMAKE_BUILD_TYPE=Release ^
  -G "Visual Studio 17 2022" -A x64
```

### 4. Build the Plugin

```batch
cmake --build . --config Release
```

### 5. Install the Plugin

```batch
cmake --install . --prefix=C:\path\to\your\godot\project\addons\usd
```

## Building for Different Platforms

### Android

Additional configuration for Android builds:

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  ...
```

### iOS

Additional configuration for iOS builds:

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake \
  -DPLATFORM=OS64 \
  ...
```

## Troubleshooting

### Common Issues

#### USD Libraries Not Found

If CMake can't find the USD libraries, explicitly set the USD_INSTALL_DIR:

```bash
cmake .. -DUSD_INSTALL_DIR=/absolute/path/to/usd/install
```

#### TBB or OpenSubdiv Not Found

Similarly, set the paths explicitly:

```bash
cmake .. -DTBB_ROOT=/absolute/path/to/tbb -DOPENSUBDIV_ROOT=/absolute/path/to/opensubdiv
```

#### Compilation Errors

If you encounter compilation errors related to USD headers:

1. Ensure you're using a compatible C++ standard (C++17 or later)
2. Check that the USD version is compatible with the plugin
3. Verify that all dependencies are correctly built and installed

## Plugin Structure

After building, the plugin structure should look like:

```
addons/usd/
├── bin/
│   ├── linux/
│   ├── macos/
│   └── windows/
├── include/
├── lib/
├── plugin.cfg
└── gdextension.gdextension
```

## Using the Plugin

1. Copy the entire `addons/usd` directory to your Godot project
2. Enable the plugin in Godot's Project Settings under the Plugins tab
3. The USD tools should now be available in the Godot editor

## Development Workflow

When developing the plugin:

1. Make your code changes
2. Rebuild using the steps above
3. Copy the updated binaries to your test Godot project
4. Test your changes in the Godot editor

For faster iteration, you can set up a symbolic link from your build output directly to your Godot project's addons directory.
