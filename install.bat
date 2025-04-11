@echo off
setlocal enabledelayedexpansion

:: Check for argument
if "%~1"=="" (
    echo Error: No Godot scene directory provided.
    echo Usage: %~nx0 ^<path_to_godot_scene_dir^>
    exit /b 1
)

set "GODOT_SCENE_DIR=%~1"

:: Check if project.godot exists
if not exist "%GODOT_SCENE_DIR%\project.godot" (
    echo Error: project.godot not found in the specified path: %GODOT_SCENE_DIR%
    exit /b 1
)

:: Create necessary directories
mkdir "%GODOT_SCENE_DIR%\addons" >nul 2>nul
xcopy /e /i /y addons "%GODOT_SCENE_DIR%\addons"

echo Successfully copied the plugin to %GODOT_SCENE_DIR%\addons\

mkdir "%GODOT_SCENE_DIR%\lib" >nul 2>nul
xcopy /e /i /y "build-usd\install\plugin" "%GODOT_SCENE_DIR%\lib\plugin"
xcopy /e /i /y "build-usd\install\lib\usd" "%GODOT_SCENE_DIR%\lib\usd"

:: Define function-like block to copy and rename
set "DLL_SOURCE_DIR=build-usd\deps\install\lib"
set "DLL_DEST_DIR=%GODOT_SCENE_DIR%\lib"

call :copy_and_flatten libosdCPU.3.6.0.dll libosdCPU.dll
call :copy_and_flatten libosdGPU.3.6.0.dll libosdGPU.dll
call :copy_and_flatten libtbb.12.15.dll libtbb.dll
call :copy_and_flatten libtbbmalloc.2.15.dll libtbbmalloc.dll
call :copy_and_flatten libtbbmalloc_proxy.2.15.dll libtbbmalloc_proxy.dll

:: Copy plugin
copy /y "bin\godot_usd.dll" "%GODOT_SCENE_DIR%\lib\"

echo Successfully copied the plugin to %GODOT_SCENE_DIR%\lib\
echo Build and installation completed successfully!
echo You can now run your Godot project with the USD plugin.
echo To use the plugin, make sure to add the 'addons' directory to your Godot project.
exit /b 0

:: Function to copy and rename DLLs
:copy_and_flatten
set "src=%DLL_SOURCE_DIR%\%1"
set "dst=%DLL_DEST_DIR%\%2"

if not exist "!dst!" (
    copy /y "!src!" "!dst!"
)
exit /b
