# =============================================================================
# Sunny.Render - DSP and Audio Processing
# =============================================================================
#
# Component: RDPK001A
# Domain: RD (Render) | Category: PK (Package)
#
# This library provides sample-accurate DSP:
# - MIDI event scheduling
# - Euclidean rhythm generation
# - Modulation (LFO, envelope, S&H)
# - Arpeggio patterns
#
# All timing uses exact rational arithmetic.
# Sample-accurate scheduling for real-time operation.
#
# =============================================================================

file(GLOB_RECURSE RENDER_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/**/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/**/*.h"
)

add_library(Sunny.Render STATIC
    ${RENDER_SOURCES}
)

target_include_directories(Sunny.Render
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/Sunny.Render>
)

# Depends on Core for theory primitives
target_link_libraries(Sunny.Render
    PUBLIC Sunny::Core
)

target_compile_features(Sunny.Render PUBLIC cxx_std_23)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Sunny.Render PRIVATE
        -Wall -Wextra -Wpedantic -Werror
    )
elseif(MSVC)
    target_compile_options(Sunny.Render PRIVATE /W4 /WX)
endif()

add_library(Sunny::Render ALIAS Sunny.Render)
