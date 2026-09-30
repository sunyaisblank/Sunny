cmake_minimum_required(VERSION 3.28)
project(SunnyForbiddenIncludeFixture LANGUAGES CXX)

include("${SUNNY_ARCHITECTURE_MODULE}")
add_library(fixture STATIC src/core/owned.cpp)
sunny_verify_layer(LAYER core TARGET fixture ALLOWED_LAYERS core)
