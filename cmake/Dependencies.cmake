find_package(nlohmann_json 3.11.3 QUIET)
if(NOT nlohmann_json_FOUND)
    # Sunny's public serialization and MCP headers expose nlohmann::json.  If
    # we had to fetch it, install its package beside Sunny so a downstream
    # find_package(Sunny) resolves the same dependency without requiring a
    # second, system-wide installation.
    set(JSON_Install ON CACHE BOOL "Install Sunny's vendored JSON dependency" FORCE)
    FetchContent_Declare(
        json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG v3.11.3
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(json)
endif()

find_package(Threads REQUIRED)

find_package(pugixml 1.14 QUIET)
if(NOT pugixml_FOUND)
    FetchContent_Declare(
        pugixml
        GIT_REPOSITORY https://github.com/zeux/pugixml.git
        GIT_TAG v1.14
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(pugixml)
endif()

if(SUNNY_BUILD_TESTS)
    find_package(Catch2 3.4 QUIET)
    if(NOT Catch2_FOUND)
        FetchContent_Declare(
            Catch2
            GIT_REPOSITORY https://github.com/catchorg/Catch2.git
            GIT_TAG v3.4.0
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(Catch2)
        list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    endif()
endif()

if(SUNNY_BUILD_PYTHON_BINDINGS)
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 2.11 CONFIG QUIET)
    if(NOT pybind11_FOUND)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
            GIT_SHALLOW TRUE
            EXCLUDE_FROM_ALL
        )
        FetchContent_MakeAvailable(pybind11)
    endif()
endif()
