# =============================================================================
# Sunny.Test - Validation Suite
# =============================================================================
#
# Component: TSPK001A
# Domain: TS (Test) | Category: PK (Package)
#
# Domain-aligned test targets mirroring source structure:
# - Sunny.Test.Core:           Tests for Sunny.Core (pitch, scale, harmony, etc.)
# - Sunny.Test.Render:         Tests for Sunny.Render (transport, modulation, arpeggio)
# - Sunny.Test.Infrastructure: Tests for Sunny.Infrastructure (protocol, bridge, MCP, etc.)
# - Sunny.Test.Max:            Tests for Sunny.Max standalone algorithms
#
# =============================================================================

include(FetchContent)

# Fetch Catch2
find_package(Catch2 3 QUIET)
if(NOT Catch2_FOUND)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )
    FetchContent_MakeAvailable(Catch2)
endif()

include(Catch)

# =============================================================================
# Sunny.Test.Core
# =============================================================================

file(GLOB_RECURSE CORE_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Sunny.Core/**/*.cpp"
)

if(CORE_TEST_SOURCES)
    add_executable(Sunny.Test.Core ${CORE_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Core PRIVATE
        Sunny::Core
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Core PRIVATE cxx_std_23)

    # nlohmann/json headers needed for Score IR serialisation tests
    if(TARGET nlohmann_json::nlohmann_json)
        get_target_property(_json_inc nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(Sunny.Test.Core PRIVATE ${_json_inc})
    endif()

    catch_discover_tests(Sunny.Test.Core
        TEST_PREFIX "Core."
        EXTRA_ARGS --reporter=console
    )
endif()

# =============================================================================
# Sunny.Test.Render
# =============================================================================

file(GLOB_RECURSE RENDER_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Sunny.Render/**/*.cpp"
)

if(RENDER_TEST_SOURCES)
    add_executable(Sunny.Test.Render ${RENDER_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Render PRIVATE
        Sunny::Core
        Sunny::Render
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Render PRIVATE cxx_std_23)

    catch_discover_tests(Sunny.Test.Render
        TEST_PREFIX "Render."
        EXTRA_ARGS --reporter=console
    )
endif()

# =============================================================================
# Sunny.Test.Infrastructure
# =============================================================================

file(GLOB_RECURSE INFRA_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Sunny.Infrastructure/**/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Sunny.Infrastructure/*.cpp"
)

if(INFRA_TEST_SOURCES)
    add_executable(Sunny.Test.Infrastructure ${INFRA_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Infrastructure PRIVATE
        Sunny::Core
        Sunny::Render
        Sunny::Infrastructure
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Infrastructure PRIVATE cxx_std_23)

    # nlohmann/json headers needed for MCP and bridge tests
    if(TARGET nlohmann_json::nlohmann_json)
        get_target_property(_json_inc nlohmann_json::nlohmann_json INTERFACE_INCLUDE_DIRECTORIES)
        target_include_directories(Sunny.Test.Infrastructure PRIVATE ${_json_inc})
    endif()

    catch_discover_tests(Sunny.Test.Infrastructure
        TEST_PREFIX "Infra."
        EXTRA_ARGS --reporter=console
    )
endif()

# =============================================================================
# Sunny.Test.Max
# =============================================================================

file(GLOB_RECURSE MAX_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Sunny.Max/**/*.cpp"
)

if(MAX_TEST_SOURCES)
    add_executable(Sunny.Test.Max ${MAX_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Max PRIVATE
        Sunny::Core
        Catch2::Catch2WithMain
    )
    target_include_directories(Sunny.Test.Max PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/../Sunny.Max"
    )
    target_compile_features(Sunny.Test.Max PRIVATE cxx_std_23)

    catch_discover_tests(Sunny.Test.Max
        TEST_PREFIX "Max."
        EXTRA_ARGS --reporter=console
    )
endif()
