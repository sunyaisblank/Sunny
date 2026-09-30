set_property(DIRECTORY PROPERTY max::external YES)
set_property(DIRECTORY PROPERTY max::glob ON)
set(BUNDLE_IDENTIFIER "sunny.lfo")
project(sunny.lfo_tilde LANGUAGES CXX)

target_compile_features(sunny.lfo_tilde PRIVATE cxx_std_23)
target_link_libraries(sunny.lfo_tilde PRIVATE sunny_max_runtime)
set_target_properties(sunny.lfo_tilde PROPERTIES CXX_STANDARD 23 CXX_STANDARD_REQUIRED ON)
