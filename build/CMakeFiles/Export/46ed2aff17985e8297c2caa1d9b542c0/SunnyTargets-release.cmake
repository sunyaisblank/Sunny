#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "Sunny::Sunny.Core" for configuration "Release"
set_property(TARGET Sunny::Sunny.Core APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Sunny::Sunny.Core PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSunny.Core.a"
  )

list(APPEND _cmake_import_check_targets Sunny::Sunny.Core )
list(APPEND _cmake_import_check_files_for_Sunny::Sunny.Core "${_IMPORT_PREFIX}/lib/libSunny.Core.a" )

# Import target "Sunny::Sunny.Render" for configuration "Release"
set_property(TARGET Sunny::Sunny.Render APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Sunny::Sunny.Render PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSunny.Render.a"
  )

list(APPEND _cmake_import_check_targets Sunny::Sunny.Render )
list(APPEND _cmake_import_check_files_for_Sunny::Sunny.Render "${_IMPORT_PREFIX}/lib/libSunny.Render.a" )

# Import target "Sunny::Sunny.Infrastructure" for configuration "Release"
set_property(TARGET Sunny::Sunny.Infrastructure APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(Sunny::Sunny.Infrastructure PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libSunny.Infrastructure.a"
  )

list(APPEND _cmake_import_check_targets Sunny::Sunny.Infrastructure )
list(APPEND _cmake_import_check_files_for_Sunny::Sunny.Infrastructure "${_IMPORT_PREFIX}/lib/libSunny.Infrastructure.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
