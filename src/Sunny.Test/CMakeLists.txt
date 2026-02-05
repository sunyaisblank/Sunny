# =============================================================================
# Sunny.Test - Validation Suite
# =============================================================================
#
# Component: TSPK001A
# Domain: TS (Test) | Category: PK (Package)
#
# Test categories:
# - Unit: 1:1 component tests (TSXX tests XX)
# - Diagnostic: Mathematical invariant verification
# - Integration: Pipeline and routing tests
# - Benchmark: Performance tracking
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

# =============================================================================
# Unit Tests
# =============================================================================

file(GLOB UNIT_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Unit/*.cpp"
)

if(UNIT_TEST_SOURCES)
    add_executable(Sunny.Test.Unit ${UNIT_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Unit PRIVATE
        Sunny::Core
        Sunny::Render
        Sunny::Infrastructure
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Unit PRIVATE cxx_std_23)

    include(Catch)
    catch_discover_tests(Sunny.Test.Unit
        TEST_PREFIX "Unit."
        EXTRA_ARGS --reporter=console
    )
endif()

# =============================================================================
# Diagnostic Tests
# =============================================================================

file(GLOB DIAGNOSTIC_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Diagnostic/*.cpp"
)

if(DIAGNOSTIC_TEST_SOURCES)
    add_executable(Sunny.Test.Diagnostic ${DIAGNOSTIC_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Diagnostic PRIVATE
        Sunny::Core
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Diagnostic PRIVATE cxx_std_23)

    catch_discover_tests(Sunny.Test.Diagnostic
        TEST_PREFIX "Diagnostic."
        EXTRA_ARGS --reporter=console
    )
endif()

# =============================================================================
# Integration Tests
# =============================================================================

file(GLOB INTEGRATION_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Integration/*.cpp"
)

if(INTEGRATION_TEST_SOURCES)
    add_executable(Sunny.Test.Integration ${INTEGRATION_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Integration PRIVATE
        Sunny::Core
        Sunny::Render
        Sunny::Infrastructure
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Integration PRIVATE cxx_std_23)

    catch_discover_tests(Sunny.Test.Integration
        TEST_PREFIX "Integration."
        EXTRA_ARGS --reporter=console
    )
endif()

# =============================================================================
# Benchmark Tests
# =============================================================================

file(GLOB BENCHMARK_TEST_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/Benchmark/*.cpp"
)

if(BENCHMARK_TEST_SOURCES)
    add_executable(Sunny.Test.Benchmark ${BENCHMARK_TEST_SOURCES})
    target_link_libraries(Sunny.Test.Benchmark PRIVATE
        Sunny::Core
        Sunny::Render
        Catch2::Catch2WithMain
    )
    target_compile_features(Sunny.Test.Benchmark PRIVATE cxx_std_23)

    catch_discover_tests(Sunny.Test.Benchmark
        TEST_PREFIX "Benchmark."
        EXTRA_ARGS --reporter=console
    )
endif()
