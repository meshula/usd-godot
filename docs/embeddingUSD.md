# Appendix: Building a Minimal USD

This guide describes how to build a minimal version of OpenUSD suitable for integration with game engines, content creation tools requiring only basic USD functionality, and other applications. It addresses common concerns about USD's build complexity by focusing on a streamlined, CMake-based approach that eliminates unnecessary components.

## Build Philosophy


1. **Pure CMake builds** - Allows easy integration to existing cmake based builds
2. **No Python dependency** - Removing Python runtime requirements
3. **Minimal dependencies** - Including only what's necessary for core USD and Hydra functionality
4. **Static, monolithic library** - Building USD as a single static library for easier integration
5. **No tests** - Excluding test code to reduce build time and size and to not clutter IDE project environments with non-game or non-application related targets

## Prerequisites

### Tools
- CMake 3.26 or newer
- C++17 compatible compiler
- Git

### Required Dependencies
Only two external dependencies are needed:
- **TBB** (Thread Building Blocks) - For USD's threading system
- **OpenSubdiv** - For subdivision surface support in Hydra

## Environment Setup

Start by setting up your environment variables:

```bash
# Set up environment variables for paths, adjust as necessary
export USD_SOURCE_DIR=~/dev/OpenUSD               # USD source code location
export USD_DEPS_DIR=/var/tmp/usd-deps             # Dependencies directory
export USD_BUILD_DIR=/var/tmp/usd-build           # USD build directory
export USD_INSTALL_DIR=${USD_BUILD_DIR}/install   # USD installation directory

# Create necessary directories
mkdir -p ${USD_DEPS_DIR}/install
mkdir -p ${USD_BUILD_DIR}
```

## Building Dependencies

### TBB (Threading Building Blocks)

```bash
cd ${USD_DEPS_DIR}

# Download and extract TBB
curl -L https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.9.0.zip --output oneTBB-2021.9.0.zip
unzip oneTBB-2021.9.0.zip && mv oneTBB-2021.9.0/ oneTBB
cd oneTBB && mkdir -p build && cd build

# Configure and build TBB
cmake .. -DTBB_TEST=OFF -DTBB_STRICT=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${USD_DEPS_DIR}/install
cmake --build . --config Release && cmake --install .
cd ../..
```

### OpenSubdiv (Minimal Build)

```bash
cd ${USD_DEPS_DIR}

# Download and extract OpenSubdiv
curl -L https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_6_0.zip --output OpenSubdiv.3.6.0.zip
unzip OpenSubdiv.3.6.0.zip && mv OpenSubdiv-3_6_0/ OpenSubdiv
cd OpenSubdiv && mkdir build && cd build

# Configure with minimal features
cmake .. -DNO_OPENGL=ON -DNO_EXAMPLES=ON -DNO_TUTORIALS=ON \
  -DNO_REGRESSION=ON -DNO_DOC=ON -DNO_OMP=ON -DNO_CUDA=ON \
  -DNO_OPENCL=ON -DNO_DX=ON -DNO_TESTS=ON -DNO_GLEW=ON \
  -DNO_GLFW=ON -DNO_PTEX=ON -DNO_TBB=ON \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=${USD_DEPS_DIR}/install

# Build and install
cmake --build . --config Release && cmake --install .
cd ../..
```

## Building USD

### Getting the Source

```bash
# Clone the USD repository
git clone https://github.com/PixarAnimationStudios/USD.git ${USD_SOURCE_DIR}
cd ${USD_SOURCE_DIR}

# Optional: Check out a specific version
# git checkout v23.11
```

### Configure and Build

```bash
cd ${USD_BUILD_DIR}

# Configure USD with minimal features
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
cmake --build . --config Release -j8 --target install
```

## Verifying the Build

To verify your USD build, you can create a small test program:

```cpp
// test_usd.cpp
#include <pxr/pxr.h>
#include <pxr/usd/usd/stage.h>
#include <iostream>

int main() {
    pxr::UsdStageRefPtr stage = pxr::UsdStage::CreateInMemory();
    pxr::UsdPrim prim = stage->DefinePrim(pxr::SdfPath("/Cube"), pxr::TfToken("Cube"));
    
    stage->Export("test.usda");
    std::cout << "USD is working correctly. Created test.usda" << std::endl;
    
    return 0;
}
```

### Compiling on Linux/macOS with GCC

```bash
g++ test_usd.cpp -o test_usd -std=c++17 \
  -I${USD_INSTALL_DIR}/include \
  -L${USD_INSTALL_DIR}/lib \
  -lusd_ms \
  -ltbb
```

### Compiling on Linux/macOS with Clang

```bash
clang++ test_usd.cpp -o test_usd -std=c++17 \
  -I${USD_INSTALL_DIR}/include \
  -L${USD_INSTALL_DIR}/lib \
  -lusd_ms \
  -ltbb
```

### Compiling on Windows with MSVC

From a Visual Studio Developer Command Prompt:

```batch
cl /EHsc /std:c++17 /Fetest_usd.exe test_usd.cpp ^
  /I"%USD_INSTALL_DIR%\include" ^
  /link /LIBPATH:"%USD_INSTALL_DIR%\lib" ^
  usd_ms.lib tbb.lib
```

Note for Windows: Ensure the USD libraries are in your PATH or in the same directory as the executable:

```batch
set PATH=%USD_INSTALL_DIR%\lib;%PATH%
```

## Integration with Applications

When integrating with your application or game engine, you'll need to:

1. Add the USD include directories to your build path
2. Link against the USD static libraries
3. Copy necessary USD plugin resources ~ where to place these will be specific to your application architecture

For engine plugins or extensions, you would include these paths in your build configuration file (e.g., `CMakeLists.txt`, `SConstruct`, `premake5.lua`, etc.).

## Troubleshooting Common Issues

### Missing TBB or OpenSubdiv

If CMake can't find TBB or OpenSubdiv, explicitly set their paths:

```bash
cmake ${USD_SOURCE_DIR} \
  ... \
  -DTBB_ROOT=${USD_DEPS_DIR}/install \
  -DOpenSubdiv_ROOT=${USD_DEPS_DIR}/install
```

### Finding Installed Libraries

If your application can't find USD libraries at runtime:

```bash
# Add to LD_LIBRARY_PATH (Linux)
export LD_LIBRARY_PATH=${USD_INSTALL_DIR}/lib:$LD_LIBRARY_PATH

# Or DYLD_LIBRARY_PATH (macOS)
export DYLD_LIBRARY_PATH=${USD_INSTALL_DIR}/lib:$DYLD_LIBRARY_PATH

# The DLLs should be in the search path on Windows.
```

## Size Optimization

The default USD build includes many optional features. Our approach already minimizes size, but if you need further optimization:

1. **Strip debug symbols** - Add `-DCMAKE_CXX_FLAGS="-Os"` to your CMake configuration
2. **Disable unused components** - Review additional `PXR_BUILD_*` and `PXR_ENABLE_*` options
3. **Optimize for size** - Use `-DCMAKE_CXX_FLAGS="-Os"` instead of the default `-O2`

### Advanced: Link Time Optimization Considerations

Link Time Optimization (LTO) or Interprocedural Optimization (IPO) can potentially reduce binary size and improve performance, but requires special care with USD:

```bash
# Enable LTO/IPO (use with caution)
cmake ... -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
```

**Important warning**: USD relies heavily on static initialization through TF_REGISTRY_FUNCTION macros and similar constructs to register types, schemas, and plugins. LTO/IPO might incorrectly eliminate these seemingly "unused" static constructors, breaking USD's type system in subtle ways.

If you decide to use LTO with USD:

1. **Test thoroughly** - Verify all USD functionality works correctly after optimization
2. **Preserve critical code** - If using GCC/Clang, consider flags like `-fno-lto-odr-type-merging`
3. **Force symbol retention** - Use linker options like `--whole-archive` (GNU ld) or `-force_load` (Apple ld) for critical USD libraries
4. **Selective application** - Consider applying LTO only to non-USD components of your project

Many production USD integrations avoid LTO entirely to prevent these complex issues.

## Integration Patterns

### Game Engines

For game engines, consider these integration approaches:

1. **Asset Pipeline Integration**: Use USD as an import/export format for your content pipeline
2. **Runtime Scene Format**: Load USD stages directly at runtime for scene composition
3. **Hydra Render Delegate**: Implement a custom render delegate to render USD through your engine's renderer
4. **Scene Index Filter Bridge**: Use Hydra's Scene Indices as an abstraction layer between USD and your engine's scene representation

### Content Creation Tools

For DCC tools and content creation applications:

1. **Native USD Editing**: Provide direct manipulation of USD scene elements
2. **Layer-Based Workflows**: Expose USD's composition and layering capabilities
3. **Schema Extensions**: Define custom schemas for application-specific data

## Conclusion

This minimal USD build approach has only two easy to build dependencies and a quick build time while providing all necessary functionality for integration with game engines and other applications. By focusing on CMake, removing Python requirements, and using a static monolithic library, we address the most common concerns about integrating USD into other projects.
