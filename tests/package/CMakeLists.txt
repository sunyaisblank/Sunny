cmake_minimum_required(VERSION 3.28)

project(SunnyPackageSmoke LANGUAGES CXX)

include(CTest)
find_package(Sunny 0.4 CONFIG REQUIRED)

add_executable(sunny_package_smoke main.cpp)
target_compile_features(sunny_package_smoke PRIVATE cxx_std_23)
target_link_libraries(sunny_package_smoke PRIVATE sunny::infrastructure sunny::max_adapter)

add_test(NAME sunny.installed-package COMMAND sunny_package_smoke)
