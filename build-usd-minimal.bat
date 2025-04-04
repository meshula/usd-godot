@echo off
setlocal enabledelayedexpansion

:: Set up environment variables for paths
set "USD_SOURCE_DIR=%CD%\build-usd\OpenUSD"
set "USD_DEPS_DIR=%CD%\build-usd\deps"
set "USD_BUILD_DIR=%CD%\build-usd\build"
set "USD_INSTALL_DIR=%CD%\build-usd\install"

:: Create necessary directories
if not exist "%USD_DEPS_DIR%\install" mkdir "%USD_DEPS_DIR%\install"
if not exist "%USD_BUILD_DIR%" mkdir "%USD_BUILD_DIR%"
if not exist "%USD_SOURCE_DIR%" mkdir "%USD_SOURCE_DIR%"

echo === Building USD and dependencies ===
echo USD_SOURCE_DIR: %USD_SOURCE_DIR%
echo USD_DEPS_DIR: %USD_DEPS_DIR%
echo USD_BUILD_DIR: %USD_BUILD_DIR%
echo USD_INSTALL_DIR: %USD_INSTALL_DIR%
echo.

:: Check for required tools
where cmake >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: cmake is required but not installed. Please install cmake.
    exit /b 1
)

where git >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: git is required but not installed. Please install git.
    exit /b 1
)

where curl >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo Error: curl is required but not installed. Please install curl.
    exit /b 1
)

:: Check for 7-Zip or PowerShell for unzipping
set "UNZIP_TOOL="
where 7z >nul 2>&1
if %ERRORLEVEL% equ 0 (
    set "UNZIP_TOOL=7z"
) else (
    echo Warning: 7-Zip not found, will try to use PowerShell for unzipping.
    echo If this fails, please install 7-Zip.
    set "UNZIP_TOOL=powershell"
)

echo === Building TBB (Threading Building Blocks) ===
cd "%USD_DEPS_DIR%"

:: Download and extract TBB if not already done
if not exist "oneTBB" (
    echo Downloading TBB...
    curl -L https://github.com/oneapi-src/oneTBB/archive/refs/tags/v2021.9.0.zip --output oneTBB-2021.9.0.zip
    
    if "%UNZIP_TOOL%"=="7z" (
        7z x oneTBB-2021.9.0.zip -y
        rename oneTBB-2021.9.0 oneTBB
    ) else (
        powershell -command "Expand-Archive -Path oneTBB-2021.9.0.zip -DestinationPath . -Force"
        rename oneTBB-2021.9.0 oneTBB
    )
)

:: Build TBB if not already built
if not exist "%USD_DEPS_DIR%\install\lib\tbb.lib" (
    echo Building TBB...
    cd oneTBB
    if not exist "build" mkdir build
    cd build
    cmake .. -DTBB_TEST=OFF -DTBB_STRICT=OFF -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%USD_DEPS_DIR%\install" -G "Visual Studio 17 2022" -A x64
    cmake --build . --config Release && cmake --install . --config Release
    cd "%USD_DEPS_DIR%"
) else (
    echo TBB already built, skipping...
)

echo === Building OpenSubdiv (Minimal Build) ===
cd "%USD_DEPS_DIR%"

:: Download and extract OpenSubdiv if not already done
if not exist "OpenSubdiv" (
    echo Downloading OpenSubdiv...
    curl -L https://github.com/PixarAnimationStudios/OpenSubdiv/archive/v3_6_0.zip --output OpenSubdiv.3.6.0.zip
    
    if "%UNZIP_TOOL%"=="7z" (
        7z x OpenSubdiv.3.6.0.zip -y
        rename OpenSubdiv-3_6_0 OpenSubdiv
    ) else (
        powershell -command "Expand-Archive -Path OpenSubdiv.3.6.0.zip -DestinationPath . -Force"
        rename OpenSubdiv-3_6_0 OpenSubdiv
    )
)

:: Build OpenSubdiv if not already built
if not exist "%USD_DEPS_DIR%\install\lib\osdCPU.lib" (
    echo Building OpenSubdiv...
    cd OpenSubdiv
    if not exist "build" mkdir build
    cd build
    cmake .. -DNO_OPENGL=ON -DNO_EXAMPLES=ON -DNO_TUTORIALS=ON ^
      -DNO_REGRESSION=ON -DNO_DOC=ON -DNO_OMP=ON -DNO_CUDA=ON ^
      -DNO_OPENCL=ON -DNO_DX=ON -DNO_TESTS=ON -DNO_GLEW=ON ^
      -DNO_GLFW=ON -DNO_PTEX=ON -DNO_TBB=ON ^
      -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="%USD_DEPS_DIR%\install" ^
      -G "Visual Studio 17 2022" -A x64
    cmake --build . --config Release && cmake --install . --config Release
    cd "%USD_DEPS_DIR%"
) else (
    echo OpenSubdiv already built, skipping...
)

echo === Getting USD Source ===
:: Clone the USD repository if not already done
if not exist "%USD_SOURCE_DIR%\.git" (
    echo Cloning USD repository...
    git clone https://github.com/PixarAnimationStudios/USD.git "%USD_SOURCE_DIR%"
    cd "%USD_SOURCE_DIR%"
    :: Check out a specific version
    git checkout dev
) else (
    echo USD repository already cloned, skipping...
)

echo === Building USD ===
cd "%USD_BUILD_DIR%"

:: Configure USD with minimal features
echo Configuring USD...
cmake "%USD_SOURCE_DIR%" ^
  -DCMAKE_INSTALL_PREFIX="%USD_INSTALL_DIR%" ^
  -DPXR_ENABLE_PYTHON_SUPPORT=OFF ^
  -DPXR_BUILD_TESTS=OFF ^
  -DPXR_BUILD_EXAMPLES=OFF ^
  -DPXR_BUILD_TUTORIALS=OFF ^
  -DPXR_BUILD_USD_TOOLS=OFF ^
  -DPXR_BUILD_IMAGING=ON ^
  -DPXR_BUILD_USD_IMAGING=ON ^
  -DPXR_BUILD_USDVIEW=OFF ^
  -DPXR_ENABLE_MATERIALX_SUPPORT=OFF ^
  -DPXR_ENABLE_HDF5_SUPPORT=OFF ^
  -DPXR_ENABLE_OPENVDB_SUPPORT=OFF ^
  -DPXR_ENABLE_OSL_SUPPORT=OFF ^
  -DPXR_ENABLE_PTEX_SUPPORT=OFF ^
  -DPXR_ENABLE_NAMESPACES=ON ^
  -DPXR_MONOLITHIC_INSTALL=ON ^
  -DBUILD_SHARED_LIBS=OFF ^
  -DPXR_ENABLE_BOOST_SUPPORT=OFF ^
  -DCMAKE_PREFIX_PATH="%USD_DEPS_DIR%\install" ^
  -DTBB_ROOT="%USD_DEPS_DIR%\install" ^
  -DCMAKE_BUILD_TYPE=Release ^
  -G "Visual Studio 17 2022" -A x64

:: Build and install USD
echo Building USD...
cmake --build . --config Release --target install

echo === USD Build Complete ===
echo USD has been built and installed to: %USD_INSTALL_DIR%
echo.
echo To build the Godot-USD plugin, set the following environment variables:
echo set USD_INSTALL_DIR=%USD_INSTALL_DIR%
echo set GODOT_SOURCE_DIR=C:\path\to\godot\source
echo.
echo Then run: build.bat

cd "%~dp0"
