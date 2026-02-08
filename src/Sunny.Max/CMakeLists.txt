cmake_minimum_required(VERSION 3.20)
project(sunny_max_externals VERSION 0.1.0 LANGUAGES CXX)

# =============================================================================
# Sunny Max Externals
# =============================================================================
#
# Component: MXPK001A
# Domain: MX (Max) | Category: PK (Package)
#
# Builds Max/MSP externals for real-time parameter control and DSP.
# Requires the Max SDK or min-devkit to be installed.
#
# Build Instructions:
#   1. Set MAX_SDK_PATH to your Max SDK location
#   2. mkdir build && cd build
#   3. cmake .. -DMAX_SDK_PATH=/path/to/max-sdk
#   4. cmake --build .
#
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# =============================================================================
# Max SDK Detection
# =============================================================================

# Try to find Max SDK in common locations
set(MAX_SDK_SEARCH_PATHS
    "$ENV{MAX_SDK_PATH}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../max-sdk"
    "${CMAKE_CURRENT_SOURCE_DIR}/max-sdk"
    "/Applications/Max 8/Contents/Resources/C74/packages/max-sdk"
    "$ENV{HOME}/Documents/Max 8/Packages/max-sdk"
    "C:/Program Files/Cycling '74/Max 8/resources/packages/max-sdk"
)

find_path(MAX_SDK_PATH
    NAMES source/c74support/max-includes/ext.h
    PATHS ${MAX_SDK_SEARCH_PATHS}
    DOC "Path to Max SDK"
)

if(NOT MAX_SDK_PATH)
    message(WARNING "Max SDK not found. Set MAX_SDK_PATH to build externals.")
    message(STATUS "Searched in: ${MAX_SDK_SEARCH_PATHS}")
    return()
endif()

message(STATUS "Found Max SDK: ${MAX_SDK_PATH}")

# =============================================================================
# Max SDK Include Paths
# =============================================================================

set(MAX_INCLUDES
    "${MAX_SDK_PATH}/source/c74support/max-includes"
    "${MAX_SDK_PATH}/source/c74support/msp-includes"
    "${MAX_SDK_PATH}/source/c74support/jit-includes"
)

# Platform-specific settings
if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    set(MAX_EXTERNAL_SUFFIX ".mxo")
    set(MAX_LINKER_FLAGS "-bundle -flat_namespace -undefined suppress")
elseif(WIN32)
    set(MAX_EXTERNAL_SUFFIX ".mxe64")
    set(MAX_LINKER_FLAGS "")
else()
    message(WARNING "Max externals not supported on this platform")
    return()
endif()

# =============================================================================
# Sunny Core Library (from parent project)
# =============================================================================

# Link to Sunny.Core if available (from parent build)
if(TARGET Sunny::Core)
    set(SUNNY_CORE_AVAILABLE TRUE)
    message(STATUS "Linking to Sunny::Core from parent project")
else()
    # Try to find from parent directory
    set(SUNNY_CORE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../Sunny.Core")
    if(EXISTS "${SUNNY_CORE_PATH}/CMakeLists.txt")
        add_subdirectory("${SUNNY_CORE_PATH}" "${CMAKE_BINARY_DIR}/Sunny.Core")
        set(SUNNY_CORE_AVAILABLE TRUE)
        message(STATUS "Found Sunny.Core library")
    else()
        set(SUNNY_CORE_AVAILABLE FALSE)
        message(STATUS "Sunny.Core not found, externals will use standalone implementations")
    endif()
endif()

# =============================================================================
# Macro for building Max externals
# =============================================================================

macro(add_max_external NAME)
    add_library(${NAME} MODULE
        ${ARGN}
    )

    target_include_directories(${NAME} PRIVATE
        ${MAX_INCLUDES}
        "${CMAKE_CURRENT_SOURCE_DIR}/../Sunny.Core"
    )

    if(SUNNY_CORE_AVAILABLE)
        target_link_libraries(${NAME} PRIVATE Sunny::Core)
        target_compile_definitions(${NAME} PRIVATE SUNNY_CORE_AVAILABLE=1)
        target_include_directories(${NAME} PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/../Sunny.Infrastructure"
        )
    endif()

    set_target_properties(${NAME} PROPERTIES
        BUNDLE TRUE
        BUNDLE_EXTENSION "mxo"
        PREFIX ""
        SUFFIX ""
        OUTPUT_NAME "${NAME}"
    )

    if(APPLE)
        set_target_properties(${NAME} PROPERTIES
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
            XCODE_ATTRIBUTE_WRAPPER_EXTENSION "mxo"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.sunny.${NAME}"
        )
    endif()
endmacro()

# =============================================================================
# Externals
# =============================================================================

# sunny.parameter~ - Real-time parameter control with sample-accurate timing
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/External/sunny.parameter_tilde.cpp")
    add_max_external(sunny.parameter~
        External/sunny.parameter_tilde.cpp
    )
endif()

# sunny.euclidean~ - Euclidean rhythm generator
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/External/sunny.euclidean_tilde.cpp")
    add_max_external(sunny.euclidean~
        External/sunny.euclidean_tilde.cpp
    )
endif()

# sunny.voicelead - Voice leading calculator (non-audio)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/External/sunny.voicelead.cpp")
    add_max_external(sunny.voicelead
        External/sunny.voicelead.cpp
    )
endif()

# =============================================================================
# Installation
# =============================================================================

# Install to Max packages directory
if(APPLE)
    set(MAX_PACKAGES_DIR "$ENV{HOME}/Documents/Max 8/Packages/Sunny")
elseif(WIN32)
    set(MAX_PACKAGES_DIR "$ENV{USERPROFILE}/Documents/Max 8/Packages/Sunny")
endif()

install(DIRECTORY Abstraction/
    DESTINATION "${MAX_PACKAGES_DIR}/patchers"
    FILES_MATCHING PATTERN "*.maxpat"
)

# Install externals
install(TARGETS sunny.parameter~ sunny.euclidean~ sunny.voicelead
    DESTINATION "${MAX_PACKAGES_DIR}/externals"
    OPTIONAL
)
