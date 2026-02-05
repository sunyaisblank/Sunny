# =============================================================================
# Sunny.Core - Music Theory Primitives
# =============================================================================
#
# Component: CRPK001A
# Domain: CR (Core) | Category: PK (Package)
#
# This library provides the foundational music theory computation:
# - Pitch class operations (Z/12Z group)
# - Scale definitions and generation
# - Harmony analysis (functional, negative)
# - Voice leading algorithms
# - Rhythm generation (Euclidean, polyrhythm)
# - Beat/timing exact arithmetic
#
# All operations use exact integer arithmetic where possible.
# No floating-point in pitch/rhythm critical paths.
#
# =============================================================================

# Collect all source files by subsystem
file(GLOB_RECURSE PITCH_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Pitch/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Pitch/*.h"
)

file(GLOB_RECURSE SCALE_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Scale/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Scale/*.h"
)

file(GLOB_RECURSE HARMONY_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Harmony/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Harmony/*.h"
)

file(GLOB_RECURSE VOICELEADING_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/VoiceLeading/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/VoiceLeading/*.h"
)

file(GLOB_RECURSE RHYTHM_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Rhythm/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Rhythm/*.h"
)

file(GLOB_RECURSE TENSOR_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Tensor/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Tensor/*.h"
)

file(GLOB_RECURSE CONSTANTS_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Constants/*.h"
)

# Create static library
add_library(Sunny.Core STATIC
    ${PITCH_SOURCES}
    ${SCALE_SOURCES}
    ${HARMONY_SOURCES}
    ${VOICELEADING_SOURCES}
    ${RHYTHM_SOURCES}
    ${TENSOR_SOURCES}
    ${CONSTANTS_SOURCES}
)

# Include directories
target_include_directories(Sunny.Core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include/Sunny.Core>
)

# C++20 required
target_compile_features(Sunny.Core PUBLIC cxx_std_23)

# Strict warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(Sunny.Core PRIVATE
        -Wall -Wextra -Wpedantic -Werror
        -Wno-unused-parameter  # Allow for interface consistency
    )
elseif(MSVC)
    target_compile_options(Sunny.Core PRIVATE /W4 /WX)
endif()

# Alias for cleaner usage
add_library(Sunny::Core ALIAS Sunny.Core)
