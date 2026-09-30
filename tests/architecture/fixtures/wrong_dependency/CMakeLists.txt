cmake_minimum_required(VERSION 3.28)
project(SunnyWrongDependencyFixture LANGUAGES CXX)

include("${SUNNY_ARCHITECTURE_MODULE}")
add_library(sunny_core STATIC src/core/core.cpp)
add_library(sunny::core ALIAS sunny_core)
add_library(fixture STATIC src/render/render.cpp)
target_link_libraries(fixture PRIVATE sunny::core)
sunny_verify_layer(LAYER render TARGET fixture ALLOWED_LAYERS core render)
