cmake_minimum_required(VERSION 3.28)

if(NOT APPLE AND NOT WIN32)
    message(FATAL_ERROR "Cycling '74 Max externals are supported only on macOS and Windows")
endif()

set(MAX_SDK_PACKAGE_OUT_OF_TREE ON CACHE BOOL "Stage a distributable Max package" FORCE)

include(FetchContent)
FetchContent_Declare(
    max-sdk-base
    GIT_REPOSITORY https://github.com/Cycling74/max-sdk-base.git
    GIT_TAG c03a2922a2a8ff149165a1e2ea134e9321d6e202
)
FetchContent_MakeAvailable(max-sdk-base)

project(SunnyMaxPackage VERSION 0.4.0 LANGUAGES CXX)

# Keep bundle metadata independent of the surrounding checkout's Git
# description, and avoid the SDK's placeholder author domain.
set(GIT_VERSION_TAG "${PROJECT_VERSION}")
set(AUTHOR_DOMAIN "com.github.sunyaisblank")

find_package(Threads REQUIRED)

add_library(
    sunny_max_runtime STATIC
    ../src/max/clock_adapter.cpp
    ../src/max/control_transfer.cpp
    ../src/max/itm_event_adapter.cpp
    ../src/max/modulation_adapter.cpp
    ../src/render/block_clock.cpp
    ../src/render/modulation.cpp
    ../src/render/scheduled_event.cpp
    ../src/render/signal_block.cpp
)
target_compile_features(sunny_max_runtime PUBLIC cxx_std_23)
target_include_directories(sunny_max_runtime PUBLIC ../include)
target_link_libraries(sunny_max_runtime PUBLIC Threads::Threads)
set_target_properties(
    sunny_max_runtime
    PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON POSITION_INDEPENDENT_CODE ON
)

add_subdirectory(source/sunny.lfo_tilde)
add_subdirectory(source/sunny.adsr_tilde)
add_subdirectory(source/sunny.hold_tilde)
add_subdirectory(source/sunny.clock_tilde)
add_subdirectory(source/sunny.events)

if(APPLE)
    set(_sunny_max_platform macos)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm64|aarch64)$")
        set(_sunny_max_architecture arm64)
    else()
        set(_sunny_max_architecture x86_64)
    endif()
else()
    set(_sunny_max_platform windows)
    set(_sunny_max_architecture x86_64)
endif()
set(
    SUNNY_MAX_ARCHIVE
    "${CMAKE_BINARY_DIR}/Sunny-${PROJECT_VERSION}-${_sunny_max_platform}-${_sunny_max_architecture}.zip"
    CACHE FILEPATH
    "Distributable Sunny Max package archive"
)
add_custom_target(
    sunny_max_archive
    COMMAND
        "${CMAKE_COMMAND}"
        "-DSUNNY_MAX_STAGED_PACKAGE=${CMAKE_BINARY_DIR}/package"
        "-DSUNNY_MAX_ARCHIVE=${SUNNY_MAX_ARCHIVE}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/CreateArchive.cmake"
    DEPENDS
        sunny.lfo_tilde
        sunny.adsr_tilde
        sunny.hold_tilde
        sunny.clock_tilde
        sunny.events
    BYPRODUCTS "${SUNNY_MAX_ARCHIVE}" "${SUNNY_MAX_ARCHIVE}.sha256"
    VERBATIM
)
