# =============================================================================
# Sunny.Infrastructure - Application and Communication Layer
# =============================================================================
#
# Component: INPK001A
# Domain: IN (Infrastructure) | Category: PK (Package)
#
# This library provides application services:
# - Operation orchestration
# - Ableton LOM bridge
# - TCP/UDP/OSC transport
# - Session state machine
# - Python bindings (pybind11)
#
# =============================================================================

file(GLOB_RECURSE INFRA_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Application/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Application/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bridge/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Bridge/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Transport/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Transport/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Session/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Session/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocol/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Protocol/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/RealTime/*.h"
)

add_library(Sunny.Infrastructure STATIC
    ${INFRA_SOURCES}
)

target_include_directories(Sunny.Infrastructure
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/Sunny.Infrastructure>
)

# Depends on Core and Render
target_link_libraries(Sunny.Infrastructure
    PUBLIC
        Sunny::Core
        Sunny::Render
)

# nlohmann/json — header-only, used only internally.
# Use include_directories instead of target_link to avoid export issues.
if(TARGET nlohmann_json::nlohmann_json)
    get_target_property(_json_inc nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
    target_include_directories(Sunny.Infrastructure PRIVATE ${_json_inc})
endif()

target_compile_features(Sunny.Infrastructure PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Sunny.Infrastructure PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
elseif(MSVC)
    target_compile_options(Sunny.Infrastructure PRIVATE /W4 /WX)
endif()

add_library(Sunny::Infrastructure ALIAS Sunny.Infrastructure)

# =============================================================================
# Python Bindings (optional)
# =============================================================================

option(SUNNY_BUILD_PYTHON_BINDINGS "Build Python bindings" ON)

if(SUNNY_BUILD_PYTHON_BINDINGS)
    find_package(Python COMPONENTS Interpreter Development REQUIRED)
    find_package(pybind11 CONFIG QUIET)

    if(NOT pybind11_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    pybind11_add_module(sunny_native
        "${CMAKE_CURRENT_SOURCE_DIR}/Binding/BDPY001A.cpp"
    )

    target_link_libraries(sunny_native PRIVATE
        Sunny::Infrastructure
    )

    # Install Python module
    install(TARGETS sunny_native
        LIBRARY DESTINATION ${CMAKE_INSTALL_PREFIX}/lib/python
    )
endif()
