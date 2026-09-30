if(NOT DEFINED SUNNY_MAX_EVIDENCE_TOOL OR NOT DEFINED SUNNY_SOURCE_DIR OR
   NOT DEFINED SUNNY_BINARY_DIR)
    message(FATAL_ERROR "Max evidence CLI test requires tool, source, and binary paths")
endif()

set(_fixture "${SUNNY_BINARY_DIR}/tests/max-evidence-cli-fixture")
set(_package_root "${_fixture}/package")
set(_evidence_root "${_fixture}/evidence")
set(_archive "${_fixture}/Sunny-package.zip")
set(_record "${_fixture}/record.json")
file(REMOVE_RECURSE "${_fixture}")
file(MAKE_DIRECTORY "${_package_root}" "${_evidence_root}")
file(WRITE "${_archive}" "abc")

foreach(_object IN ITEMS sunny.lfo~ sunny.adsr~ sunny.hold~ sunny.clock~ sunny.events)
    set(_binary "${_package_root}/externals/${_object}.mxo/Contents/MacOS/${_object}")
    get_filename_component(_binary_directory "${_binary}" DIRECTORY)
    file(MAKE_DIRECTORY "${_binary_directory}")
    file(WRITE "${_binary}" "abc")
endforeach()

execute_process(
    COMMAND
        "${SUNNY_MAX_EVIDENCE_TOOL}"
        assemble
        "${SUNNY_SOURCE_DIR}/max-package/misc/validation/max-validation-observation.example.json"
        "${_archive}"
        "${_package_root}"
        "${_evidence_root}"
    RESULT_VARIABLE _assemble_result
    OUTPUT_VARIABLE _assembled
    ERROR_VARIABLE _assemble_error
)
if(NOT _assemble_result EQUAL 0)
    message(FATAL_ERROR "Max evidence assembly failed: ${_assemble_error}")
endif()
file(WRITE "${_record}" "${_assembled}")

execute_process(
    COMMAND
        "${SUNNY_MAX_EVIDENCE_TOOL}"
        verify
        "${_record}"
        "${_archive}"
        "${_package_root}"
        "${_evidence_root}"
    RESULT_VARIABLE _verify_result
    OUTPUT_VARIABLE _verify_output
    ERROR_VARIABLE _verify_error
)
if(NOT _verify_result EQUAL 0 OR NOT _verify_output STREQUAL "verified\n")
    message(FATAL_ERROR "Max evidence verification failed: ${_verify_error}")
endif()

file(WRITE "${_archive}" "changed")
execute_process(
    COMMAND
        "${SUNNY_MAX_EVIDENCE_TOOL}"
        verify
        "${_record}"
        "${_archive}"
        "${_package_root}"
        "${_evidence_root}"
    RESULT_VARIABLE _tamper_result
    ERROR_VARIABLE _tamper_error
)
if(_tamper_result EQUAL 0 OR NOT _tamper_error MATCHES "digest_mismatch: package_archive")
    message(FATAL_ERROR "Max evidence verifier accepted archive tampering: ${_tamper_error}")
endif()

file(REMOVE_RECURSE "${_fixture}")
