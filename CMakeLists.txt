cmake_minimum_required(VERSION 3.28)

project(
    Sunny
    VERSION 0.4.0
    DESCRIPTION "Composable music intelligence for Ableton Live"
    LANGUAGES CXX
)

include(GNUInstallDirs)
include(FetchContent)
include(CMakePackageConfigHelpers)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(SUNNY_BUILD_TESTS "Build the C++ test suite" ON)
option(SUNNY_BUILD_PYTHON_BINDINGS "Build the Python extension" ON)
option(SUNNY_BUILD_MCP_SERVER "Build the MCP server" ON)
option(SUNNY_MULL_INSTRUMENT "Instrument Sunny targets with Mull" OFF)

# Both native and deployed Python peers consume this one machine-readable
# authority.  A configure-time generated public header keeps installed C++
# consumers on the same contract as the self-contained Remote Script package.
set(_sunny_bridge_contract
    "${PROJECT_SOURCE_DIR}/remote_script/Sunny/bridge_contract.json"
)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_sunny_bridge_contract}")
file(READ "${_sunny_bridge_contract}" _sunny_bridge_contract_json)
string(JSON _sunny_bridge_contract_size LENGTH "${_sunny_bridge_contract_json}")
if(NOT _sunny_bridge_contract_size EQUAL 2)
    message(FATAL_ERROR "Sunny bridge contract must contain exactly two version fields")
endif()
string(
    JSON SUNNY_BRIDGE_PROTOCOL_VERSION_VALUE
    GET "${_sunny_bridge_contract_json}" bridge_protocol_version
)
string(
    JSON SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION_VALUE
    GET "${_sunny_bridge_contract_json}" target_snapshot_schema_version
)
foreach(
    _sunny_contract_version
    SUNNY_BRIDGE_PROTOCOL_VERSION_VALUE
    SUNNY_TARGET_SNAPSHOT_SCHEMA_VERSION_VALUE
)
    if(NOT "${${_sunny_contract_version}}" MATCHES "^[1-9][0-9]*$" OR
       ${${_sunny_contract_version}} GREATER 4294967295)
        message(FATAL_ERROR "${_sunny_contract_version} must be a positive uint32")
    endif()
endforeach()
set(_sunny_generated_include_dir "${PROJECT_BINARY_DIR}/generated/include")
include(cmake/BridgeBundle.cmake)
sunny_prepare_bridge_bundle(
    "${PROJECT_SOURCE_DIR}/remote_script/Sunny"
    "${PROJECT_BINARY_DIR}/remote_script/Sunny"
    SUNNY_BRIDGE_SOURCE_SHA256_VALUE
)
file(MAKE_DIRECTORY "${_sunny_generated_include_dir}/sunny/infrastructure/ableton")
configure_file(
    cmake/version.hpp.in
    "${_sunny_generated_include_dir}/sunny/version.hpp"
    @ONLY
)
configure_file(
    cmake/bridge_contract.hpp.in
    "${_sunny_generated_include_dir}/sunny/infrastructure/ableton/bridge_contract.hpp"
    @ONLY
)

include(cmake/Dependencies.cmake)
include(cmake/Warnings.cmake)
include(cmake/Architecture.cmake)

add_library(sunny_core STATIC)
add_library(sunny::core ALIAS sunny_core)
set_target_properties(sunny_core PROPERTIES EXPORT_NAME core)
target_compile_features(sunny_core PUBLIC cxx_std_23)
target_include_directories(
    sunny_core
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(
    sunny_core
    PUBLIC
        Threads::Threads
        $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
)
target_sources(
    sunny_core
    PRIVATE
        src/core/corpus/analysis.cpp
        src/core/corpus/serialization.cpp
        src/core/corpus/validation.cpp
        src/core/corpus/workflows.cpp
        src/core/form/motif.cpp
        src/core/form/structure.cpp
        src/core/harmony/applied_chords.cpp
        src/core/harmony/cadence.cpp
        src/core/harmony/chord_scale.cpp
        src/core/harmony/chromatic.cpp
        src/core/harmony/harmonic_function.cpp
        src/core/harmony/negative_harmony.cpp
        src/core/harmony/non_tertian.cpp
        src/core/harmony/progression.cpp
        src/core/harmony/roman_numeral.cpp
        src/core/melody/analysis.cpp
        src/core/mix/serialization.cpp
        src/core/mix/validation.cpp
        src/core/mix/workflows.cpp
        src/core/pitch/diatonic_interval.cpp
        src/core/pitch/dihedral_group.cpp
        src/core/pitch/pitch_class.cpp
        src/core/pitch/pitch_class_set.cpp
        src/core/pitch/spelled_pitch.cpp
        src/core/post_tonal/twelve_tone.cpp
        src/core/project/validation.cpp
        src/core/rhythm/euclidean.cpp
        src/core/rhythm/meter.cpp
        src/core/rhythm/tuplet.cpp
        src/core/scale/definitions.cpp
        src/core/scale/generation.cpp
        src/core/scale/relationships.cpp
        src/core/score/harmony_analysis.cpp
        src/core/score/midi_compiler.cpp
        src/core/score/mutations.cpp
        src/core/score/projection.cpp
        src/core/score/queries.cpp
        src/core/score/score_document.cpp
        src/core/score/serialization.cpp
        src/core/score/serialization_primitives.cpp
        src/core/score/time.cpp
        src/core/score/tuning.cpp
        src/core/score/validation.cpp
        src/core/score/views.cpp
        src/core/score/workflows.cpp
        src/core/timbre/serialization.cpp
        src/core/timbre/live_capabilities.cpp
        src/core/timbre/validation.cpp
        src/core/timbre/workflows.cpp
        src/core/transform/k_net.cpp
        src/core/transform/neo_riemannian.cpp
        src/core/tuning/historical_temperament.cpp
        src/core/voice_leading/constraints.cpp
        src/core/voice_leading/counterpoint.cpp
        src/core/voice_leading/figured_bass.cpp
        src/core/voice_leading/voice_leading.cpp
)
sunny_enable_warnings(sunny_core)

add_library(sunny_render STATIC)
add_library(sunny::render ALIAS sunny_render)
set_target_properties(sunny_render PROPERTIES EXPORT_NAME render)
target_compile_features(sunny_render PUBLIC cxx_std_23)
target_include_directories(
    sunny_render
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(sunny_render PUBLIC sunny::core)
target_sources(
    sunny_render
    PRIVATE
        src/render/arpeggiator.cpp
        src/render/block_event_scheduler.cpp
        src/render/block_clock.cpp
        src/render/modulation.cpp
        src/render/scheduled_event.cpp
        src/render/signal_block.cpp
        src/render/transport.cpp
)
sunny_enable_warnings(sunny_render)

add_library(sunny_max_adapter STATIC)
add_library(sunny::max_adapter ALIAS sunny_max_adapter)
set_target_properties(sunny_max_adapter PROPERTIES EXPORT_NAME max_adapter)
target_compile_features(sunny_max_adapter PUBLIC cxx_std_23)
target_include_directories(
    sunny_max_adapter
    PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(sunny_max_adapter PUBLIC sunny::render)
target_sources(
    sunny_max_adapter
    PRIVATE
        src/max/clock_adapter.cpp
        src/max/control_transfer.cpp
        src/max/itm_event_adapter.cpp
        src/max/modulation_adapter.cpp
)
sunny_enable_warnings(sunny_max_adapter)

add_library(sunny_infrastructure STATIC)
add_library(sunny::infrastructure ALIAS sunny_infrastructure)
set_target_properties(sunny_infrastructure PROPERTIES EXPORT_NAME infrastructure)
target_compile_features(sunny_infrastructure PUBLIC cxx_std_23)
target_include_directories(
    sunny_infrastructure
    PUBLIC
        $<BUILD_INTERFACE:${_sunny_generated_include_dir}>
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(
    sunny_infrastructure
    PUBLIC
        sunny::core
        sunny::render
        $<BUILD_INTERFACE:nlohmann_json::nlohmann_json>
    PRIVATE
        $<BUILD_INTERFACE:pugixml::pugixml>
)
target_sources(
    sunny_infrastructure
    PRIVATE
        src/infrastructure/ableton/dispatcher.cpp
        src/infrastructure/ableton/deployment.cpp
        src/infrastructure/ableton/lom_protocol.cpp
        src/infrastructure/ableton/managed_realization.cpp
        src/infrastructure/ableton/target_snapshot.cpp
        src/infrastructure/ableton/target_profile.cpp
        src/infrastructure/ableton/transport.cpp
        src/infrastructure/compilation_workflows.cpp
        src/infrastructure/corpus/ingestion.cpp
        src/infrastructure/formats/abc.cpp
        src/infrastructure/formats/ableton_evidence.cpp
        src/infrastructure/formats/ableton_project.cpp
        src/infrastructure/formats/ableton_mix.cpp
        src/infrastructure/formats/ableton_score.cpp
        src/infrastructure/formats/ableton_timbre.cpp
        src/infrastructure/formats/lilypond.cpp
        src/infrastructure/formats/midi_file.cpp
        src/infrastructure/formats/musicxml.cpp
        src/infrastructure/formats/scala.cpp
        src/infrastructure/formats/score_to_lilypond.cpp
        src/infrastructure/formats/score_to_musicxml.cpp
        src/infrastructure/mcp/core_tools.cpp
        src/infrastructure/mcp/corpus_tools.cpp
        src/infrastructure/mcp/mix_tools.cpp
        src/infrastructure/mcp/project_tools.cpp
        src/infrastructure/mcp/project_session.cpp
        src/infrastructure/mcp/project_timeline.cpp
        src/infrastructure/mcp/workspace_state.cpp
        src/infrastructure/mcp/workspace_tools.cpp
        src/infrastructure/mcp/evidence_encoding.cpp
        src/infrastructure/mcp/score_tools.cpp
        src/infrastructure/mcp/score_notation_tools.cpp
        src/infrastructure/mcp/server.cpp
        src/infrastructure/mcp/timbre_tools.cpp
        src/infrastructure/orchestrator.cpp
)
sunny_enable_warnings(sunny_infrastructure)

sunny_verify_layer(
    LAYER core
    TARGET sunny_core
    ALLOWED_LAYERS core
)
sunny_verify_layer(
    LAYER render
    TARGET sunny_render
    ALLOWED_LAYERS core render
    LOCAL_TARGET_DEPENDENCIES sunny::core
)
sunny_verify_layer(
    LAYER max
    TARGET sunny_max_adapter
    ALLOWED_LAYERS core render max
    LOCAL_TARGET_DEPENDENCIES sunny::render
)
sunny_verify_layer(
    LAYER infrastructure
    TARGET sunny_infrastructure
    ALLOWED_LAYERS core render infrastructure
    LOCAL_TARGET_DEPENDENCIES sunny::core sunny::render
)
sunny_verify_repository_metadata(
    PROJECT_VERSION "${PROJECT_VERSION}"
    BRIDGE_PROTOCOL_VERSION "${SUNNY_BRIDGE_PROTOCOL_VERSION_VALUE}"
)

if(SUNNY_MULL_INSTRUMENT)
    set(_sunny_mull_flags
        -fpass-plugin=/usr/lib/mull-ir-frontend-18
        -g
        -grecord-command-line
    )
    target_compile_options(sunny_core PRIVATE ${_sunny_mull_flags})
    target_compile_options(sunny_render PRIVATE ${_sunny_mull_flags})
    target_compile_options(sunny_max_adapter PRIVATE ${_sunny_mull_flags})
    target_compile_options(sunny_infrastructure PRIVATE ${_sunny_mull_flags})
endif()

if(SUNNY_BUILD_MCP_SERVER)
    add_executable(sunny-mcp apps/sunny_mcp.cpp)
    target_link_libraries(sunny-mcp PRIVATE sunny::infrastructure)
    sunny_enable_warnings(sunny-mcp)
endif()

if(SUNNY_BUILD_PYTHON_BINDINGS)
    pybind11_add_module(sunny_native src/bindings/python_module.cpp)
    target_link_libraries(sunny_native PRIVATE sunny::infrastructure)
    set_target_properties(
        sunny_native
        PROPERTIES LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/python"
    )
    sunny_enable_warnings(sunny_native)
endif()

if(SUNNY_BUILD_TESTS)
    include(CTest)
    add_subdirectory(tests)
endif()

install(
    TARGETS sunny_core sunny_render sunny_max_adapter sunny_infrastructure
    EXPORT SunnyTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
if(TARGET sunny-mcp)
    install(TARGETS sunny-mcp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
if(TARGET sunny_native)
    if(SKBUILD)
        set(_sunny_python_install_dir ".")
    else()
        set(_sunny_python_install_dir "${CMAKE_INSTALL_LIBDIR}/python")
    endif()
    install(
        TARGETS sunny_native
        LIBRARY DESTINATION "${_sunny_python_install_dir}"
            COMPONENT SunnyPython
        RUNTIME DESTINATION "${_sunny_python_install_dir}"
            COMPONENT SunnyPython
    )
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(
    FILES
        "${_sunny_generated_include_dir}/sunny/version.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sunny"
)
install(
    FILES
        "${_sunny_generated_include_dir}/sunny/infrastructure/ableton/bridge_contract.hpp"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sunny/infrastructure/ableton"
)

set(_sunny_cmake_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/Sunny")
configure_package_config_file(
    cmake/SunnyConfig.cmake.in
    "${PROJECT_BINARY_DIR}/SunnyConfig.cmake"
    INSTALL_DESTINATION "${_sunny_cmake_install_dir}"
)
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/SunnyConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(
    EXPORT SunnyTargets
    FILE SunnyTargets.cmake
    NAMESPACE sunny::
    DESTINATION "${_sunny_cmake_install_dir}"
)
install(
    FILES
        "${PROJECT_BINARY_DIR}/SunnyConfig.cmake"
        "${PROJECT_BINARY_DIR}/SunnyConfigVersion.cmake"
    DESTINATION "${_sunny_cmake_install_dir}"
)

message(
    STATUS
        "Sunny ${PROJECT_VERSION}: C++${CMAKE_CXX_STANDARD}, tests=${SUNNY_BUILD_TESTS}, "
        "Python=${SUNNY_BUILD_PYTHON_BINDINGS}, MCP=${SUNNY_BUILD_MCP_SERVER}"
)
