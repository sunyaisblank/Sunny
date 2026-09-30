cmake_minimum_required(VERSION 3.28)

project(SunnyPackageSmoke LANGUAGES CXX)

include(CTest)
find_package(Sunny 0.4 CONFIG REQUIRED)
find_program(
    SUNNY_MAX_EVIDENCE_EXECUTABLE
    NAMES sunny-max-evidence
    HINTS ${CMAKE_PREFIX_PATH}
    PATH_SUFFIXES bin
    NO_DEFAULT_PATH
    REQUIRED
)

add_executable(sunny_package_smoke main.cpp)
target_compile_features(sunny_package_smoke PRIVATE cxx_std_23)
target_link_libraries(sunny_package_smoke PRIVATE sunny::infrastructure sunny::max_adapter)

add_test(NAME sunny.installed-package COMMAND sunny_package_smoke)
add_test(NAME sunny.installed-max-evidence-tool COMMAND "${SUNNY_MAX_EVIDENCE_EXECUTABLE}" --help)
