cmake_minimum_required(VERSION 3.26)
project(godot-usd VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set RPATH for finding libraries at runtime
if(APPLE)
    set(CMAKE_INSTALL_RPATH "@loader_path")
elseif(UNIX)
    set(CMAKE_INSTALL_RPATH "$ORIGIN")
endif()

# Set default build type if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "Build type" FORCE)
endif()

# Options
option(USD_STATIC_LIBS "Link USD statically" ON)

# Check required paths
if(NOT DEFINED USD_INSTALL_DIR)
    message(FATAL_ERROR "USD_INSTALL_DIR must be defined")
endif()

if(NOT DEFINED GODOT_SOURCE_DIR)
    message(FATAL_ERROR "GODOT_SOURCE_DIR must be defined. Set it to the path where Godot engine source code is located.")
endif()

# Include ExternalProject module
include(ExternalProject)

# Set godot-cpp directories
set(GODOT_CPP_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp")
set(GODOT_CPP_BINARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp")

# Determine the correct godot-cpp library name based on platform
if(APPLE)
    set(GODOT_CPP_LIB "${GODOT_CPP_BINARY_DIR}/bin/libgodot-cpp.macos.template_release.universal.a")
elseif(UNIX)
    # Linux - detect architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(GODOT_CPP_LIB "${GODOT_CPP_BINARY_DIR}/bin/libgodot-cpp.linux.template_release.x86_64.a")
    else()
        set(GODOT_CPP_LIB "${GODOT_CPP_BINARY_DIR}/bin/libgodot-cpp.linux.template_release.x86_32.a")
    endif()
elseif(WIN32)
    # Windows - detect architecture
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(GODOT_CPP_LIB "${GODOT_CPP_BINARY_DIR}/bin/libgodot-cpp.windows.template_release.x86_64.a")
    else()
        set(GODOT_CPP_LIB "${GODOT_CPP_BINARY_DIR}/bin/libgodot-cpp.windows.template_release.x86_32.a")
    endif()
endif()

message(STATUS "Expected godot-cpp library: ${GODOT_CPP_LIB}")

# Check if godot-cpp is already cloned
if(NOT EXISTS "${GODOT_CPP_SOURCE_DIR}")
    # Clone godot-cpp repository
    message(STATUS "Cloning godot-cpp repository...")
    execute_process(
        COMMAND git clone --depth 1 --branch godot-4.4-stable https://github.com/godotengine/godot-cpp.git ${GODOT_CPP_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone godot-cpp repository")
    endif()
endif()

# Create an interface library for Godot extension interface
add_library(godot_extension_interface INTERFACE)
target_include_directories(godot_extension_interface INTERFACE
    ${GODOT_SOURCE_DIR}/core/extension
)

# Check if godot-cpp is already built
if(NOT EXISTS "${GODOT_CPP_LIB}")
    # Build godot-cpp using SCons
    message(STATUS "Building godot-cpp...")
    execute_process(
        COMMAND scons target=template_release generate_bindings=yes
        WORKING_DIRECTORY ${GODOT_CPP_SOURCE_DIR}
        RESULT_VARIABLE SCONS_RESULT
    )
    if(NOT SCONS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build godot-cpp")
    endif()
else()
    message(STATUS "godot-cpp already built skipping build step")
endif()

# Create an imported library for godot-cpp
add_library(godot-cpp STATIC IMPORTED)
set_target_properties(godot-cpp PROPERTIES
    IMPORTED_LOCATION "${GODOT_CPP_LIB}"
)

# Source files
set(SOURCES
    src/register_types.cpp
    src/register_types.h
    src/usd_plugin.cpp
    src/usd_plugin.h
    src/usd_document.cpp
    src/usd_document.h
    src/usd_export_settings.cpp
    src/usd_export_settings.h
    src/usd_state.cpp
    src/usd_state.h
    src/usd_mesh_import_helper.cpp
    src/usd_mesh_import_helper.h
    src/usd_mesh_export_helper.cpp
    src/usd_mesh_export_helper.h
    src/usd_stage_proxy.cpp
    src/usd_stage_proxy.h
    src/usd_prim_proxy.cpp
    src/usd_prim_proxy.h
    src/usd_stage_manager.cpp
    src/usd_stage_manager.h
    src/mcp_server.cpp
    src/mcp_server.h
    src/mcp_json.h
    src/version.h
)

# Create the shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Set CMAKE_PREFIX_PATH to include the USD dependencies
list(APPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build-usd/deps/install")

# Find required packages for USD
find_package(TBB REQUIRED)
find_package(OpenGL REQUIRED)
find_package(OpenSubdiv REQUIRED)

# Include the USD cmake targets
include(${USD_INSTALL_DIR}/cmake/pxrTargets.cmake)

# Include directories for godot-cpp
target_include_directories(${PROJECT_NAME} PRIVATE
    ${GODOT_CPP_SOURCE_DIR}/include
    ${GODOT_CPP_SOURCE_DIR}/gen/include
)

# Link against godot-cpp godot_extension_interface and USD
target_link_directories(${PROJECT_NAME} PRIVATE
    ${GODOT_CPP_BINARY_DIR}/bin
)

if(USD_STATIC_LIBS)
    # Link against individual USD libraries using the pxr targets
    target_link_libraries(${PROJECT_NAME} PRIVATE
        godot-cpp
        godot_extension_interface
        usd
        usdGeom
        usdImaging
        usdImagingGL
        hd
        hdSt
        hdx
        gf
        sdf
        tf
        vt
        ar
        kind
        pcp
        plug
        work
        usdShade
        usdLux
        usdSkel
        usdUtils
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE godot-cpp godot_extension_interface usd)
endif()

# Set output directory and RPATH settings
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    BUILD_WITH_INSTALL_RPATH TRUE
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

# Platform-specific RPATH settings
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "@loader_path"
        MACOSX_RPATH TRUE
    )
elseif(UNIX)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        INSTALL_RPATH "$ORIGIN"
    )
endif()

# Installation
install(TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# Install GDExtension and plugin configuration files
install(FILES
    godot-usd.gdextension
    plugin.cfg
    plugin.gd
    DESTINATION .
)

# Install library to lib directory
install(DIRECTORY bin/
    DESTINATION lib
)

# Create addon structure
install(DIRECTORY DESTINATION addons/godot-usd)
install(FILES
    plugin.cfg
    plugin.gd
    DESTINATION addons/godot-usd
)

# Create a modified gdextension file for the addon that points to the correct location
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/godot-usd.gdextension
    ${CMAKE_CURRENT_BINARY_DIR}/godot-usd-addon.gdextension
    COPYONLY
)
install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/godot-usd-addon.gdextension
    DESTINATION addons/godot-usd
    RENAME godot-usd.gdextension
)
