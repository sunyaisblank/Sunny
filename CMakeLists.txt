# =============================================================================
# Sunny - Music Theory and DAW Integration System
# =============================================================================
#
# Sunny provides music theory computation and Ableton Live integration
# via MCP (Model Context Protocol) for AI-assisted composition.
#
# Architecture follows Sirius design patterns:
# - Sunny.Core:           Music theory primitives (C++)
# - Sunny.Render:         DSP and audio processing (C++)
# - Sunny.Infrastructure: Application and communication (C++)
# - Sunny.Test:           Validation suite (C++)
#
# Build:
#   mkdir build && cd build
#   cmake .. -DCMAKE_BUILD_TYPE=Release
#   cmake --build .
#   ctest --output-on-failure
#
# =============================================================================

cmake_minimum_required(VERSION 3.20)
project(Sunny
    VERSION 0.1.0
    DESCRIPTION "Music Theory and DAW Integration System"
    LANGUAGES CXX
)

# =============================================================================
# Project Configuration
# =============================================================================

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Export compile commands for IDE support
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build type default
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# =============================================================================
# Options
# =============================================================================

option(SUNNY_BUILD_TESTS "Build test suite" ON)
option(SUNNY_BUILD_PYTHON_BINDINGS "Build Python bindings" ON)
option(SUNNY_BUILD_MAX "Build Max/MSP externals" OFF)
option(SUNNY_BUILD_BENCHMARKS "Build benchmarks" OFF)

# =============================================================================
# Dependencies
# =============================================================================

include(FetchContent)

# Catch2 for testing
if(SUNNY_BUILD_TESTS)
    find_package(Catch2 3 QUIET)
    if(NOT Catch2_FOUND)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.4.0
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    endif()
endif()

# pybind11 for Python bindings
if(SUNNY_BUILD_PYTHON_BINDINGS)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)
    find_package(pybind11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
        )
        FetchContent_MakeAvailable(pybind11)
    endif()
endif()

# =============================================================================
# Source Layers
# =============================================================================

# Core library (music theory primitives)
add_subdirectory(src/Sunny.Core)

# Render library (DSP, audio processing)
add_subdirectory(src/Sunny.Render)

# Infrastructure library (application, communication)
add_subdirectory(src/Sunny.Infrastructure)

# Test suite
if(SUNNY_BUILD_TESTS)
    enable_testing()
    include(CTest)
    add_subdirectory(src/Sunny.Test)
endif()

# =============================================================================
# Max/MSP Externals (optional)
# =============================================================================

if(SUNNY_BUILD_MAX)
    add_subdirectory(max)
endif()

# =============================================================================
# Installation
# =============================================================================

include(GNUInstallDirs)

install(TARGETS Sunny.Core Sunny.Render Sunny.Infrastructure
    EXPORT SunnyTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY
    src/Sunny.Core/
    src/Sunny.Render/
    src/Sunny.Infrastructure/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Sunny
    FILES_MATCHING PATTERN "*.h"
)

install(EXPORT SunnyTargets
    FILE SunnyTargets.cmake
    NAMESPACE Sunny::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Sunny
)

# =============================================================================
# Summary
# =============================================================================

message(STATUS "")
message(STATUS "Sunny Configuration:")
message(STATUS "  Version:           ${PROJECT_VERSION}")
message(STATUS "  Build type:        ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard:      ${CMAKE_CXX_STANDARD}")
message(STATUS "  Tests:             ${SUNNY_BUILD_TESTS}")
message(STATUS "  Python bindings:   ${SUNNY_BUILD_PYTHON_BINDINGS}")
message(STATUS "  Max externals:     ${SUNNY_BUILD_MAX}")
message(STATUS "")
