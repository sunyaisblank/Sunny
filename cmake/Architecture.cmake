include_guard(GLOBAL)

include(CMakeParseArguments)

# Verify one native layer against the repository's directional dependency model.
#
# The check intentionally runs while CMake configures.  A source file cannot be
# added to the tree, omitted from its owning target, or made to include an
# upstream layer without making the build invalid first.
function(sunny_verify_layer)
    cmake_parse_arguments(
        ARG
        ""
        "LAYER;TARGET"
        "ALLOWED_LAYERS;LOCAL_TARGET_DEPENDENCIES"
        ${ARGN}
    )

    if(ARG_UNPARSED_ARGUMENTS OR NOT ARG_LAYER OR NOT ARG_TARGET)
        message(FATAL_ERROR "sunny_verify_layer received an invalid argument set")
    endif()
    if(NOT TARGET "${ARG_TARGET}")
        message(FATAL_ERROR "Sunny architecture target does not exist: ${ARG_TARGET}")
    endif()

    file(
        GLOB_RECURSE _sunny_layer_public_headers
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/sunny/${ARG_LAYER}/*.hpp"
    )
    file(
        GLOB_RECURSE _sunny_layer_private_headers
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/${ARG_LAYER}/*.hpp"
    )
    file(
        GLOB_RECURSE _sunny_layer_sources
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/src/${ARG_LAYER}/*.cpp"
    )

    set(_sunny_layer_files
        ${_sunny_layer_public_headers}
        ${_sunny_layer_private_headers}
        ${_sunny_layer_sources}
    )
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${_sunny_layer_files}
    )
    foreach(_sunny_file IN LISTS _sunny_layer_files)
        file(READ "${_sunny_file}" _sunny_contents)
        string(
            REGEX MATCHALL
            "#[ \t]*include[ \t]*<sunny/[a-z_]+/[^>\r\n]+>"
            _sunny_public_includes
            "${_sunny_contents}"
        )
        foreach(_sunny_include IN LISTS _sunny_public_includes)
            string(
                REGEX REPLACE
                ".*<sunny/([a-z_]+)/.*"
                "\\1"
                _sunny_included_layer
                "${_sunny_include}"
            )
            if(NOT _sunny_included_layer IN_LIST ARG_ALLOWED_LAYERS)
                file(RELATIVE_PATH _sunny_relative_file "${PROJECT_SOURCE_DIR}" "${_sunny_file}")
                message(
                    FATAL_ERROR
                    "Sunny layer violation: ${_sunny_relative_file} includes "
                    "${_sunny_included_layer}, but ${ARG_LAYER} may include only "
                    "[${ARG_ALLOWED_LAYERS}]"
                )
            endif()
        endforeach()
    endforeach()

    get_target_property(_sunny_declared_sources "${ARG_TARGET}" SOURCES)
    if(NOT _sunny_declared_sources)
        set(_sunny_declared_sources "")
    endif()
    set(_sunny_declared_absolute_sources "")
    foreach(_sunny_source IN LISTS _sunny_declared_sources)
        if(_sunny_source MATCHES "^\\$<")
            continue()
        endif()
        cmake_path(
            ABSOLUTE_PATH _sunny_source
            BASE_DIRECTORY "${PROJECT_SOURCE_DIR}"
            NORMALIZE
            OUTPUT_VARIABLE _sunny_absolute_source
        )
        list(APPEND _sunny_declared_absolute_sources "${_sunny_absolute_source}")
        set(_sunny_repository_source_root "${PROJECT_SOURCE_DIR}/src")
        set(_sunny_owned_source_root "${PROJECT_SOURCE_DIR}/src/${ARG_LAYER}")
        cmake_path(
            IS_PREFIX _sunny_repository_source_root
            "${_sunny_absolute_source}"
            NORMALIZE
            _sunny_is_repository_source
        )
        cmake_path(
            IS_PREFIX _sunny_owned_source_root
            "${_sunny_absolute_source}"
            NORMALIZE
            _sunny_is_owned_source
        )
        if(_sunny_is_repository_source AND NOT _sunny_is_owned_source)
            file(
                RELATIVE_PATH
                _sunny_relative_source
                "${PROJECT_SOURCE_DIR}"
                "${_sunny_absolute_source}"
            )
            message(
                FATAL_ERROR
                "Sunny source ownership violation: target ${ARG_TARGET} declares foreign source "
                "${_sunny_relative_source}"
            )
        endif()
    endforeach()
    foreach(_sunny_source IN LISTS _sunny_layer_sources)
        if(NOT _sunny_source IN_LIST _sunny_declared_absolute_sources)
            file(RELATIVE_PATH _sunny_relative_source "${PROJECT_SOURCE_DIR}" "${_sunny_source}")
            message(
                FATAL_ERROR
                "Sunny source ownership violation: ${_sunny_relative_source} is not owned by "
                "target ${ARG_TARGET}"
            )
        endif()
    endforeach()

    get_target_property(_sunny_link_libraries "${ARG_TARGET}" LINK_LIBRARIES)
    if(NOT _sunny_link_libraries)
        set(_sunny_link_libraries "")
    endif()
    set(_sunny_actual_local_dependencies "")
    foreach(_sunny_dependency IN LISTS _sunny_link_libraries)
        if(_sunny_dependency MATCHES "^sunny::[a-z_]+$")
            list(APPEND _sunny_actual_local_dependencies "${_sunny_dependency}")
        endif()
    endforeach()
    list(REMOVE_DUPLICATES _sunny_actual_local_dependencies)
    list(SORT _sunny_actual_local_dependencies)
    set(_sunny_expected_local_dependencies ${ARG_LOCAL_TARGET_DEPENDENCIES})
    list(REMOVE_DUPLICATES _sunny_expected_local_dependencies)
    list(SORT _sunny_expected_local_dependencies)
    if(NOT "${_sunny_actual_local_dependencies}" STREQUAL
       "${_sunny_expected_local_dependencies}")
        message(
            FATAL_ERROR
            "Sunny target dependency violation: ${ARG_TARGET} links local targets "
            "[${_sunny_actual_local_dependencies}], expected "
            "[${_sunny_expected_local_dependencies}]"
        )
    endif()
endfunction()

# Keep current-facing documentation and package metadata tied to machine build
# authorities. Historical protocol numbers in the decision record are outside
# this check by design.
function(sunny_verify_repository_metadata)
    cmake_parse_arguments(
        ARG
        ""
        "PROJECT_VERSION;BRIDGE_PROTOCOL_VERSION;SNAPSHOT_SCHEMA_VERSION"
        ""
        ${ARGN}
    )
    if(ARG_UNPARSED_ARGUMENTS OR NOT ARG_PROJECT_VERSION OR
       NOT ARG_BRIDGE_PROTOCOL_VERSION OR NOT ARG_SNAPSHOT_SCHEMA_VERSION)
        message(FATAL_ERROR "sunny_verify_repository_metadata received an invalid argument set")
    endif()

    set(_sunny_metadata_files
        "${PROJECT_SOURCE_DIR}/pyproject.toml"
        "${PROJECT_SOURCE_DIR}/python/sunny/__init__.py"
        "${PROJECT_SOURCE_DIR}/max-package/CMakeLists.txt"
        "${PROJECT_SOURCE_DIR}/max-package/package-info.json"
        "${PROJECT_SOURCE_DIR}/docs/reference.md"
    )
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${_sunny_metadata_files}
    )
    file(READ "${PROJECT_SOURCE_DIR}/pyproject.toml" _sunny_pyproject)
    file(READ "${PROJECT_SOURCE_DIR}/python/sunny/__init__.py" _sunny_python_package)
    file(READ "${PROJECT_SOURCE_DIR}/max-package/CMakeLists.txt" _sunny_max_cmake)
    file(READ "${PROJECT_SOURCE_DIR}/max-package/package-info.json" _sunny_max_manifest)
    if(NOT _sunny_pyproject MATCHES
       "(^|\n)version[ \t]*=[ \t]*\"${ARG_PROJECT_VERSION}\"(\r?\n|$)" OR
       NOT _sunny_python_package MATCHES
       "(^|\n)__version__[ \t]*=[ \t]*\"${ARG_PROJECT_VERSION}\"(\r?\n|$)" OR
       NOT _sunny_max_cmake MATCHES
       "project\\(SunnyMaxPackage VERSION ${ARG_PROJECT_VERSION} LANGUAGES CXX\\)" OR
       NOT _sunny_max_manifest MATCHES
       "\"version\"[ \t]*:[ \t]*\"${ARG_PROJECT_VERSION}\"")
        message(
            FATAL_ERROR
            "Sunny version authority mismatch: CMake, Python metadata, and Max package metadata "
            "must all identify ${ARG_PROJECT_VERSION}"
        )
    endif()

    set(_sunny_reference "${PROJECT_SOURCE_DIR}/docs/reference.md")
    file(READ "${_sunny_reference}" _sunny_reference_contents)
    string(
        REGEX MATCHALL
        "\"bridge_protocol_version\"[ \t]*:[ \t]*[0-9]+"
        _sunny_documented_protocol_fields
        "${_sunny_reference_contents}"
    )
    foreach(_sunny_field IN LISTS _sunny_documented_protocol_fields)
        if(NOT _sunny_field MATCHES ":[ \t]*${ARG_BRIDGE_PROTOCOL_VERSION}$")
            message(
                FATAL_ERROR
                "Sunny bridge documentation mismatch in docs/reference.md: ${_sunny_field}; "
                "authority is ${ARG_BRIDGE_PROTOCOL_VERSION}"
            )
        endif()
    endforeach()
    if(NOT _sunny_reference_contents MATCHES
       "Bridge protocol version ${ARG_BRIDGE_PROTOCOL_VERSION} returns:")
        message(
            FATAL_ERROR
            "docs/reference.md does not identify current bridge protocol "
            "${ARG_BRIDGE_PROTOCOL_VERSION}"
        )
    endif()
    if(NOT _sunny_reference_contents MATCHES
       "snapshot schema ${ARG_SNAPSHOT_SCHEMA_VERSION}([^0-9]|$)")
        message(
            FATAL_ERROR
            "docs/reference.md does not identify current snapshot schema "
            "${ARG_SNAPSHOT_SCHEMA_VERSION}"
        )
    endif()

    # The conformance guide has a normative current-contract section followed
    # by a historical decision ledger.  Pin only explicit current markers so
    # old protocol/schema numbers remain valid evidence instead of being
    # mechanically rewritten during every bridge evolution.
    set(_sunny_ableton_conformance
        "${PROJECT_SOURCE_DIR}/docs/ableton-max-conformance.md"
    )
    if(EXISTS "${_sunny_ableton_conformance}")
        set_property(
            DIRECTORY
            APPEND
            PROPERTY CMAKE_CONFIGURE_DEPENDS "${_sunny_ableton_conformance}"
        )
        file(READ "${_sunny_ableton_conformance}" _sunny_ableton_conformance_contents)
        if(NOT _sunny_ableton_conformance_contents MATCHES
           "### 2\\.1 Closed bridge algebra \\(protocol v${ARG_BRIDGE_PROTOCOL_VERSION}\\)" OR
           NOT _sunny_ableton_conformance_contents MATCHES
           "response carries `bridge_protocol_version: ${ARG_BRIDGE_PROTOCOL_VERSION}`" OR
           NOT _sunny_ableton_conformance_contents MATCHES
           "starting/final schema-${ARG_SNAPSHOT_SCHEMA_VERSION} topology")
            message(
                FATAL_ERROR
                "Sunny Ableton conformance documentation mismatch: current protocol/schema "
                "markers must identify v${ARG_BRIDGE_PROTOCOL_VERSION}/schema "
                "${ARG_SNAPSHOT_SCHEMA_VERSION}"
            )
        endif()
    endif()

    # Current native bridge comments and diagnostics are part of the protocol
    # contract too.  Historical decision/conformance prose is intentionally not
    # scanned, but a stale literal in executable infrastructure must not survive
    # a protocol upgrade.
    file(
        GLOB_RECURSE _sunny_current_bridge_sources
        CONFIGURE_DEPENDS
        "${PROJECT_SOURCE_DIR}/include/sunny/infrastructure/*.hpp"
        "${PROJECT_SOURCE_DIR}/remote_script/*.py"
        "${PROJECT_SOURCE_DIR}/src/infrastructure/*.cpp"
        "${PROJECT_SOURCE_DIR}/src/infrastructure/*.hpp"
        "${PROJECT_SOURCE_DIR}/tests/python/*.py"
    )
    set_property(
        DIRECTORY
        APPEND
        PROPERTY CMAKE_CONFIGURE_DEPENDS ${_sunny_current_bridge_sources}
    )
    foreach(_sunny_source IN LISTS _sunny_current_bridge_sources)
        file(READ "${_sunny_source}" _sunny_source_contents)
        string(
            REGEX MATCHALL
            "[Pp]rotocol-v[0-9]+|[Bb]ridge protocol v[0-9]+|requires_v[0-9]+_envelope|test_v[0-9]+_operation"
            _sunny_source_protocol_literals
            "${_sunny_source_contents}"
        )
        foreach(_sunny_literal IN LISTS _sunny_source_protocol_literals)
            string(REGEX MATCH "[0-9]+" _sunny_literal_version "${_sunny_literal}")
            if(NOT _sunny_literal_version STREQUAL "${ARG_BRIDGE_PROTOCOL_VERSION}")
                file(RELATIVE_PATH _sunny_relative_source "${PROJECT_SOURCE_DIR}" "${_sunny_source}")
                message(
                    FATAL_ERROR
                    "Sunny bridge source protocol mismatch in ${_sunny_relative_source}: "
                    "${_sunny_literal}; authority is ${ARG_BRIDGE_PROTOCOL_VERSION}"
                )
            endif()
        endforeach()
    endforeach()
endfunction()
