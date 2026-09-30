set_property(DIRECTORY PROPERTY max::external YES)
set_property(DIRECTORY PROPERTY max::glob ON)
set(BUNDLE_IDENTIFIER "sunny.events")
project(sunny.events LANGUAGES CXX)

target_compile_features(sunny.events PRIVATE cxx_std_23)
target_link_libraries(sunny.events PRIVATE sunny_max_runtime)
set_target_properties(sunny.events PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
