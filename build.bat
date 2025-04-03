@echo off
setlocal enabledelayedexpansion

:: Check if USD_INSTALL_DIR is defined
if "%USD_INSTALL_DIR%"=="" (
    echo Error: USD_INSTALL_DIR environment variable is not set.
    echo Please set it to the path where USD is installed.
    echo Example: set USD_INSTALL_DIR=C:\path\to\usd\install
    exit /b 1
)

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake
cmake .. ^
  -DUSD_INSTALL_DIR=%USD_INSTALL_DIR% ^
  -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
  -G "Visual Studio 17 2022" -A x64

:: Build
cmake --build . --config RelWithDebInfo

:: Install to local addons directory
cmake --install . --prefix=../ --config RelWithDebInfo

echo.
echo Build completed successfully!
echo The plugin has been installed to the 'addons/godot-usd' directory.
echo To use the plugin, copy the 'addons' directory to your Godot project.

cd ..
