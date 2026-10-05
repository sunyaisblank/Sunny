option(SUNNY_RELEASE_BUILD "Use and verify the release dependency lock" OFF)
set(_sunny_dependency_shallow TRUE)
if(SUNNY_RELEASE_BUILD)
    set(_sunny_dependency_shallow FALSE)
    set(_sunny_build_inputs "${PROJECT_SOURCE_DIR}/release/build-inputs.json")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_sunny_build_inputs}")
    file(READ "${_sunny_build_inputs}" _sunny_build_inputs_json)
    string(JSON _sunny_build_inputs_schema GET "${_sunny_build_inputs_json}" build_inputs_schema_version)
    if(NOT _sunny_build_inputs_schema EQUAL 1)
        message(FATAL_ERROR "Unsupported Sunny release build-inputs schema")
    endif()
endif()

function(sunny_dependency_lock name default_repository default_tag repository_output tag_output)
    if(SUNNY_RELEASE_BUILD)
        string(JSON _repository GET "${_sunny_build_inputs_json}" dependencies "${name}" repository)
        string(JSON _tag GET "${_sunny_build_inputs_json}" dependencies "${name}" revision)
        string(LENGTH "${_tag}" _tag_length)
        if(NOT _tag_length EQUAL 40 OR NOT _tag MATCHES "^[0-9a-f]+$")
            message(FATAL_ERROR "Sunny release dependency ${name} needs a complete commit revision")
        endif()
    else()
        set(_repository "${default_repository}")
        set(_tag "${default_tag}")
    endif()
    set(${repository_output} "${_repository}" PARENT_SCOPE)
    set(${tag_output} "${_tag}" PARENT_SCOPE)
endfunction()

function(sunny_verify_dependency name directory revision)
    if(SUNNY_RELEASE_BUILD)
        find_package(Git REQUIRED)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${directory}" rev-parse HEAD
            RESULT_VARIABLE _status OUTPUT_VARIABLE _actual OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND "${GIT_EXECUTABLE}" -C "${directory}" status --porcelain
            RESULT_VARIABLE _dirty_status OUTPUT_VARIABLE _dirty OUTPUT_STRIP_TRAILING_WHITESPACE)
        if(NOT _status EQUAL 0 OR NOT "${_actual}" STREQUAL "${revision}" OR
           NOT _dirty_status EQUAL 0 OR NOT _dirty STREQUAL "")
            message(FATAL_ERROR "Sunny release dependency ${name} does not match clean locked revision ${revision}")
        endif()
    endif()
endfunction()

if(NOT SUNNY_RELEASE_BUILD)
    find_package(nlohmann_json 3.11.3 QUIET)
endif()
if(SUNNY_RELEASE_BUILD OR NOT nlohmann_json_FOUND)
    # Sunny's public serialization and MCP headers expose nlohmann::json.  If
    # we had to fetch it, install its package beside Sunny so a downstream
    # find_package(Sunny) resolves the same dependency without requiring a
    # second, system-wide installation.
    set(JSON_Install ON CACHE BOOL "Install Sunny's vendored JSON dependency" FORCE)
    sunny_dependency_lock(json https://github.com/nlohmann/json.git v3.11.3 _json_repository _json_tag)
    FetchContent_Declare(
        json
        GIT_REPOSITORY "${_json_repository}"
        GIT_TAG "${_json_tag}"
        GIT_SHALLOW "${_sunny_dependency_shallow}"
    )
    FetchContent_MakeAvailable(json)
    sunny_verify_dependency(json "${json_SOURCE_DIR}" "${_json_tag}")
endif()

find_package(Threads REQUIRED)

if(NOT SUNNY_RELEASE_BUILD)
    find_package(pugixml 1.14 QUIET)
endif()
if(SUNNY_RELEASE_BUILD OR NOT pugixml_FOUND)
    sunny_dependency_lock(pugixml https://github.com/zeux/pugixml.git v1.14 _pugi_repository _pugi_tag)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY "${_pugi_repository}"
        GIT_TAG "${_pugi_tag}"
        GIT_SHALLOW "${_sunny_dependency_shallow}"
    )
    FetchContent_MakeAvailable(pugixml)
    sunny_verify_dependency(pugixml "${pugixml_SOURCE_DIR}" "${_pugi_tag}")
endif()

if(SUNNY_BUILD_TESTS)
    if(NOT SUNNY_RELEASE_BUILD)
        find_package(Catch2 3.4 QUIET)
    endif()
    if(SUNNY_RELEASE_BUILD OR NOT Catch2_FOUND)
        sunny_dependency_lock(Catch2 https://github.com/catchorg/Catch2.git v3.4.0 _catch_repository _catch_tag)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY "${_catch_repository}"
            GIT_TAG "${_catch_tag}"
            GIT_SHALLOW "${_sunny_dependency_shallow}"
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(Catch2)
        sunny_verify_dependency(Catch2 "${catch2_SOURCE_DIR}" "${_catch_tag}")
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
endif()

if(SUNNY_BUILD_PYTHON_BINDINGS)
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    if(NOT SUNNY_RELEASE_BUILD)
        find_package(pybind11 2.11 CONFIG QUIET)
    endif()
    if(SUNNY_RELEASE_BUILD OR NOT pybind11_FOUND)
        sunny_dependency_lock(pybind11 https://github.com/pybind/pybind11.git v2.11.1 _pybind_repository _pybind_tag)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY "${_pybind_repository}"
            GIT_TAG "${_pybind_tag}"
            GIT_SHALLOW "${_sunny_dependency_shallow}"
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(pybind11)
        sunny_verify_dependency(pybind11 "${pybind11_SOURCE_DIR}" "${_pybind_tag}")
    endif()
endif()
