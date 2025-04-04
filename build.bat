@echo off
setlocal enabledelayedexpansion

:: Check if USD_INSTALL_DIR is defined
if "%USD_INSTALL_DIR%"=="" (
    echo Error: USD_INSTALL_DIR environment variable is not set.
    echo Please set it to the path where USD is installed.
    echo Example: set USD_INSTALL_DIR=C:\path\to\usd\install
    exit /b 1
)

:: Check if GODOT_SOURCE_DIR is defined
if "%GODOT_SOURCE_DIR%"=="" (
    echo Error: GODOT_SOURCE_DIR environment variable is not set.
    echo Please set it to the path where Godot engine source code is located.
    echo Example: set GODOT_SOURCE_DIR=C:\path\to\godot\source
    exit /b 1
)

:: Verify that GODOT_SOURCE_DIR contains the required files
if not exist "%GODOT_SOURCE_DIR%\core\extension\gdextension_interface.h" (
    echo Error: gdextension_interface.h not found in Godot source at %GODOT_SOURCE_DIR%\core\extension\gdextension_interface.h
    echo Please make sure GODOT_SOURCE_DIR points to a valid Godot engine source directory.
    exit /b 1
)

:: Create build directory
if not exist build mkdir build
cd build

:: Configure with CMake
cmake .. ^
  -DUSD_INSTALL_DIR=%USD_INSTALL_DIR% ^
  -DGODOT_SOURCE_DIR=%GODOT_SOURCE_DIR% ^
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
