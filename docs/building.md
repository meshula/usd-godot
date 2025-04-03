# Building the Godot-USD Plugin

This guide provides instructions for building the Godot-USD integration plugin from source.

## Prerequisites

### Required Software

- **CMake** 3.26 or newer
- **C++17 compatible compiler**
  - GCC 9+ (Linux)
  - Clang 10+ (macOS)
  - MSVC 2019+ (Windows)
- **Git**
- **SCons** (for building godot-cpp)

### Required Libraries

- **OpenUSD** - See [Building a Minimal USD](embeddingUSD.md) for instructions on building a minimal version of USD suitable for this plugin
- **TBB** (Thread Building Blocks) - For USD's threading system (included when building USD)
- **OpenSubdiv** - For subdivision surface support in Hydra (included when building USD)

## Quick Start

The easiest way to build the plugin is to use the provided build scripts. You must set the USD_INSTALL_DIR environment variable to point to your USD installation:

### On Linux/macOS

```bash
# Set the path to your USD installation (required)
export USD_INSTALL_DIR=/path/to/usd/install

# Run the build script
./build.sh
```

### On Windows

```batch
# Set the path to your USD installation (required)
set USD_INSTALL_DIR=C:\path\to\usd\install

# Run the build script
build.bat
```

These scripts will:
1. Check if USD_INSTALL_DIR is set
2. Create a build directory
3. Configure the project with CMake
4. Build the plugin with RelWithDebInfo configuration
5. Install it to the local `addons/godot-usd` directory

## Manual Build Process

If you prefer to build manually or need more control over the build process, follow these steps:

### 1. Clone the Repository

```bash
git clone https://github.com/your-organization/godot-usd.git
cd godot-usd
```

### 2. Build Process on Linux/macOS

```bash
# Create build directory
mkdir -p build
cd build

# Configure with CMake
cmake .. \
  -DUSD_INSTALL_DIR=/path/to/usd/install \
  -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build . -j$(nproc)

# Install to your Godot project
cmake --install . --prefix=/path/to/your/godot/project
```

### 3. Build Process on Windows

```batch
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake .. ^
  -DUSD_INSTALL_DIR=C:\path\to\usd\install ^
  -DCMAKE_BUILD_TYPE=Release ^
  -G "Visual Studio 17 2022" -A x64

# Build
cmake --build . --config Release

# Install to your Godot project
cmake --install . --prefix=C:\path\to\your\godot\project
```

## How the Build System Works

The build system performs the following steps:

1. **Checks for godot-cpp**: If the godot-cpp library is not found, it will be automatically cloned and built
2. **Configures the build**: Sets up include paths and links to the required libraries
3. **Builds the plugin**: Compiles the C++ code into a shared library
4. **Creates the addon structure**: Organizes the files into a Godot addon structure

## Building for Different Platforms

### Android

Additional configuration for Android builds:

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=${ANDROID_NDK}/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM=android-21 \
  -DUSD_INSTALL_DIR=/path/to/android/usd/install
```

### iOS

Additional configuration for iOS builds:

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=../cmake/ios.toolchain.cmake \
  -DPLATFORM=OS64 \
  -DUSD_INSTALL_DIR=/path/to/ios/usd/install
```

## Troubleshooting

### Common Issues

#### USD Libraries Not Found

If CMake can't find the USD libraries, explicitly set the USD_INSTALL_DIR:

```bash
cmake .. -DUSD_INSTALL_DIR=/absolute/path/to/usd/install
```

#### godot-cpp Build Fails

If the automatic godot-cpp build fails, you can build it manually:

```bash
# Clone godot-cpp
git clone --recursive https://github.com/godotengine/godot-cpp.git

# Build godot-cpp
cd godot-cpp
scons target=template_debug generate_bindings=yes
cd ..

# Then configure with the path to your godot-cpp
cmake .. -DGODOT_CPP_PATH=/path/to/godot-cpp -DUSD_INSTALL_DIR=/path/to/usd/install
```

#### Compilation Errors

If you encounter compilation errors:

1. Ensure you're using a compatible C++ standard (C++17 or later)
2. Check that the USD version is compatible with the plugin
3. Verify that all dependencies are correctly built and installed

## Plugin Structure

After building, the plugin structure will look like:

```
addons/godot-usd/
├── lib/
│   ├── libgodot-usd.so (Linux)
│   ├── libgodot-usd.dylib (macOS)
│   └── godot-usd.dll (Windows)
├── godot-usd.gdextension
├── plugin.cfg
└── plugin.gd
```

## Using the Plugin

1. Copy the entire `addons/godot-usd` directory to your Godot project
2. Enable the plugin in Godot's Project Settings under the Plugins tab
3. The USD tools should now be available in the Godot editor

## Development Workflow

When developing the plugin:

1. Make your code changes
2. Run the build script (`./build.sh` or `build.bat`)
3. Copy the updated `addons/godot-usd` directory to your test Godot project
4. Test your changes in the Godot editor

For faster iteration, you can set up a symbolic link from your build output directly to your Godot project's addons directory.
