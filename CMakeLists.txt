cmake_minimum_required(VERSION 3.26)
project(godot-usd VERSION 0.1.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

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
set(GODOT_CPP_LIB "${GODOT_CPP_BINARY_DIR}/bin/libgodot-cpp.macos.template_release.universal.a")

# Check if godot-cpp is already cloned
if(NOT EXISTS "${GODOT_CPP_SOURCE_DIR}")
    # Clone godot-cpp repository
    message(STATUS "Cloning godot-cpp repository...")
    execute_process(
        COMMAND git clone --depth 1 --branch 4.2 https://github.com/godotengine/godot-cpp.git ${GODOT_CPP_SOURCE_DIR}
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
    message(STATUS "godot-cpp already built, skipping build step")
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
)

# Create the shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${GODOT_CPP_SOURCE_DIR}/include
    ${GODOT_CPP_SOURCE_DIR}/gen/include
    ${USD_INSTALL_DIR}/include
)

# Link against godot-cpp, godot_extension_interface, and USD
target_link_directories(${PROJECT_NAME} PRIVATE
    ${GODOT_CPP_BINARY_DIR}/bin
    ${USD_INSTALL_DIR}/lib
)

if(USD_STATIC_LIBS)
    # Link against individual USD libraries instead of the monolithic library
    target_link_libraries(${PROJECT_NAME} PRIVATE 
        godot-cpp 
        godot_extension_interface
        usd_usd
        usd_usdGeom
        usd_usdImaging
        usd_usdImagingGL
        usd_hd
        usd_hdSt
        usd_hdx
        usd_gf
        usd_sdf
        usd_tf
        usd_vt
        usd_ar
        usd_kind
        usd_pcp
        usd_plug
        usd_work
        usd_usdShade
        usd_usdLux
        usd_usdSkel
        usd_usdUtils
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE godot-cpp godot_extension_interface usd)
endif()

# Set output directory
set_target_properties(${PROJECT_NAME} PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/bin"
)

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
