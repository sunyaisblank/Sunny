if(NOT DEFINED SUNNY_MAX_STAGED_PACKAGE OR NOT DEFINED SUNNY_MAX_ARCHIVE)
    message(FATAL_ERROR "Sunny Max archive creation requires staged-package and archive paths")
endif()

get_filename_component(_staged "${SUNNY_MAX_STAGED_PACKAGE}" ABSOLUTE)
get_filename_component(_archive "${SUNNY_MAX_ARCHIVE}" ABSOLUTE)
if(NOT IS_DIRECTORY "${_staged}")
    message(FATAL_ERROR "Sunny Max staged package is unavailable: ${_staged}")
endif()

get_filename_component(_archive_directory "${_archive}" DIRECTORY)
# Resolve existing ancestors before creating anything: REAL_PATH alone leaves aliases
# unresolved when the destination has a nonexistent suffix.
set(_ancestor "${_archive_directory}")
while(NOT EXISTS "${_ancestor}")
    get_filename_component(_ancestor "${_ancestor}" DIRECTORY)
endwhile()
file(REAL_PATH "${_ancestor}" _resolved_ancestor)
file(RELATIVE_PATH _suffix "${_ancestor}" "${_archive}")
set(_archive "${_resolved_ancestor}/${_suffix}")
cmake_path(NORMAL_PATH _archive)
file(REAL_PATH "${_staged}" _staged)
set(_work "${_archive}.work")
set(_staged_compare "${_staged}")
set(_archive_compare "${_archive}")
set(_work_compare "${_work}")
if(WIN32)
    string(TOLOWER "${_staged_compare}" _staged_compare)
    string(TOLOWER "${_archive_compare}" _archive_compare)
    string(TOLOWER "${_work_compare}" _work_compare)
endif()
cmake_path(IS_PREFIX _staged_compare "${_archive_compare}" NORMALIZE _nested_archive)
cmake_path(IS_PREFIX _work_compare "${_staged_compare}" NORMALIZE _work_contains_staged)
if(_nested_archive OR _work_contains_staged OR IS_SYMLINK "${_work}")
    message(FATAL_ERROR
        "Sunny Max archive and work paths must be outside the staged package and disjoint from it")
endif()
if(IS_SYMLINK "${_archive}.lock")
    message(FATAL_ERROR "Sunny Max archive lock path must not be a symlink")
endif()
get_filename_component(_archive_directory "${_archive}" DIRECTORY)
file(MAKE_DIRECTORY "${_archive_directory}")
# Keep one archive/checksum publication under one owner. The lock file is build metadata.
file(LOCK "${_archive}.lock" GUARD PROCESS TIMEOUT 30 RESULT_VARIABLE _lock_result)
if(NOT _lock_result STREQUAL "0")
    message(FATAL_ERROR "Cannot lock Sunny Max archive destination: ${_lock_result}")
endif()

if(EXISTS "${_archive}" OR EXISTS "${_archive}.sha256" OR
   IS_SYMLINK "${_archive}" OR IS_SYMLINK "${_archive}.sha256")
    if(NOT EXISTS "${_archive}" OR NOT EXISTS "${_archive}.sha256" OR
       IS_DIRECTORY "${_archive}" OR IS_DIRECTORY "${_archive}.sha256" OR
       IS_SYMLINK "${_archive}" OR IS_SYMLINK "${_archive}.sha256")
        message(FATAL_ERROR "Sunny Max archive destination contains an incomplete or non-regular pair")
    endif()
endif()

file(REMOVE_RECURSE "${_work}")
file(MAKE_DIRECTORY "${_work}/Sunny")
file(COPY "${_staged}/" DESTINATION "${_work}/Sunny")
set(_candidate "${_work}/candidate.zip")
execute_process(
    COMMAND
        # Explicit --mtime fixes only modification time. SOURCE_DATE_EPOCH
        # fixes access/creation/birth times too, including ZIP extra fields.
        "${CMAKE_COMMAND}" -E env SOURCE_DATE_EPOCH=315532800 TZ=UTC
        "${CMAKE_COMMAND}" -E chdir "${_work}"
        "${CMAKE_COMMAND}" -E tar cf "${_candidate}" --format=zip
        Sunny
    RESULT_VARIABLE _archive_result
    ERROR_VARIABLE _archive_error
)
if(NOT _archive_result EQUAL 0)
    file(REMOVE_RECURSE "${_work}")
    message(FATAL_ERROR "Sunny Max archive creation failed: ${_archive_error}")
endif()

file(SHA256 "${_candidate}" _archive_sha256)
get_filename_component(_archive_name "${_archive}" NAME)
set(_checksum "${_archive_sha256}  ${_archive_name}\n")
file(WRITE "${_work}/candidate.sha256" "${_checksum}")

if(EXISTS "${_archive}")
    file(SHA256 "${_archive}" _existing_sha256)
    file(READ "${_archive}.sha256" _existing_checksum)
    file(REMOVE_RECURSE "${_work}")
    if(_existing_sha256 STREQUAL _archive_sha256 AND _existing_checksum STREQUAL _checksum)
        message(STATUS "Retained identical Sunny Max package archive: ${_archive}")
        return()
    endif()
    message(FATAL_ERROR
        "Sunny Max archive destination already contains different bytes; "
        "choose a new SUNNY_MAX_ARCHIVE path or deliberately remove the old pair")
endif()

# New outputs can briefly be incomplete to readers, which must require both files. Neither rename
# may overwrite a file created by another actor, and failure never removes an earlier valid pair.
file(RENAME "${_candidate}" "${_archive}" NO_REPLACE RESULT _publish_result)
if(NOT _publish_result STREQUAL "0")
    file(REMOVE_RECURSE "${_work}")
    message(FATAL_ERROR "Cannot publish Sunny Max archive: ${_publish_result}")
endif()
file(RENAME "${_work}/candidate.sha256" "${_archive}.sha256" NO_REPLACE RESULT _publish_result)
if(NOT _publish_result STREQUAL "0")
    file(REMOVE "${_archive}")
    file(REMOVE_RECURSE "${_work}")
    message(FATAL_ERROR "Cannot publish Sunny Max archive checksum: ${_publish_result}")
endif()
file(REMOVE_RECURSE "${_work}")
message(STATUS "Created Sunny Max package archive: ${_archive}")
