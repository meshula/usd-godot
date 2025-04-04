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

# Include FetchContent module
include(FetchContent)

# Fetch godot-cpp
FetchContent_Declare(
    godot_cpp
    GIT_REPOSITORY https://github.com/godotengine/godot-cpp.git
    GIT_TAG 4.2
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(godot_cpp)

# Create an interface library for Godot extension interface
add_library(godot_extension_interface INTERFACE)
target_include_directories(godot_extension_interface INTERFACE 
    ${GODOT_SOURCE_DIR}/core/extension
)

# Build godot-cpp using SCons
message(STATUS "Building godot-cpp...")
execute_process(
    COMMAND scons target=template_release generate_bindings=yes
    WORKING_DIRECTORY ${godot_cpp_SOURCE_DIR}
    RESULT_VARIABLE SCONS_RESULT
)
if(NOT SCONS_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to build godot-cpp")
endif()

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
    ${godot_cpp_SOURCE_DIR}/include
    ${godot_cpp_SOURCE_DIR}/gen/include
    ${USD_INSTALL_DIR}/include
)

# Link against godot-cpp, godot_extension_interface, and USD
target_link_directories(${PROJECT_NAME} PRIVATE
    ${godot_cpp_BINARY_DIR}/bin
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

# Create addon structure
install(DIRECTORY DESTINATION addons/godot-usd)
install(FILES 
    godot-usd.gdextension
    plugin.cfg
    plugin.gd
    DESTINATION addons/godot-usd
)
install(DIRECTORY bin/
    DESTINATION addons/godot-usd/lib
)
