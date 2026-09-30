cmake_minimum_required(VERSION 3.28)

project(SunnyMaxSdkHeaderCheck LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
    max-sdk-base
    GIT_REPOSITORY https://github.com/Cycling74/max-sdk-base.git
    GIT_TAG c03a2922a2a8ff149165a1e2ea134e9321d6e202
)
FetchContent_MakeAvailable(max-sdk-base)

get_filename_component(SUNNY_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)

add_library(
    sunny_max_sdk_header_check OBJECT
    "${SUNNY_SOURCE_DIR}/max-package/source/sunny.lfo_tilde/sunny.lfo_tilde.cpp"
    "${SUNNY_SOURCE_DIR}/max-package/source/sunny.adsr_tilde/sunny.adsr_tilde.cpp"
    "${SUNNY_SOURCE_DIR}/max-package/source/sunny.hold_tilde/sunny.hold_tilde.cpp"
    "${SUNNY_SOURCE_DIR}/max-package/source/sunny.clock_tilde/sunny.clock_tilde.cpp"
    "${SUNNY_SOURCE_DIR}/max-package/source/sunny.events/sunny.events.cpp"
)
target_compile_features(sunny_max_sdk_header_check PRIVATE cxx_std_23)
target_include_directories(sunny_max_sdk_header_check PRIVATE "${SUNNY_SOURCE_DIR}/include")
target_include_directories(
    sunny_max_sdk_header_check SYSTEM PRIVATE
    "${max-sdk-base_SOURCE_DIR}/c74support/max-includes"
    "${max-sdk-base_SOURCE_DIR}/c74support/msp-includes"
)
set_target_properties(
    sunny_max_sdk_header_check
    PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON
)

if(MSVC)
    target_compile_options(sunny_max_sdk_header_check PRIVATE /W4 /WX)
else()
    # Max's C API intentionally casts typed callbacks to its unprototyped
    # `method` registration type.
    target_compile_options(
        sunny_max_sdk_header_check PRIVATE
        -Wall -Wextra -Wpedantic -Werror -Wno-cast-function-type
    )
endif()
