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

# Define paths
if(NOT DEFINED GODOT_CPP_PATH)
    set(GODOT_CPP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/godot-cpp" CACHE PATH "Path to the godot-cpp library")
endif()

if(NOT DEFINED USD_INSTALL_DIR)
    message(FATAL_ERROR "USD_INSTALL_DIR must be defined")
endif()

# Check if godot-cpp exists, if not clone it
if(NOT EXISTS "${GODOT_CPP_PATH}/CMakeLists.txt")
    message(STATUS "godot-cpp not found, cloning from GitHub...")
    execute_process(
        COMMAND git clone --recursive https://github.com/godotengine/godot-cpp.git ${GODOT_CPP_PATH}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_RESULT
    )
    if(NOT GIT_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to clone godot-cpp repository")
    endif()
    
    # Build godot-cpp
    message(STATUS "Building godot-cpp...")
    execute_process(
        COMMAND scons target=template_debug generate_bindings=yes
        WORKING_DIRECTORY ${GODOT_CPP_PATH}
        RESULT_VARIABLE SCONS_RESULT
    )
    if(NOT SCONS_RESULT EQUAL 0)
        message(FATAL_ERROR "Failed to build godot-cpp")
    endif()
endif()

# Include directories
include_directories(
    ${GODOT_CPP_PATH}/include
    ${GODOT_CPP_PATH}/gen/include
    ${USD_INSTALL_DIR}/include
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

# Link against godot-cpp and USD
target_link_directories(${PROJECT_NAME} PRIVATE
    ${GODOT_CPP_PATH}/bin
    ${USD_INSTALL_DIR}/lib
)

if(USD_STATIC_LIBS)
    target_link_libraries(${PROJECT_NAME} PRIVATE godot-cpp.template.debug usd_ms)
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE godot-cpp.template.debug usd)
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
