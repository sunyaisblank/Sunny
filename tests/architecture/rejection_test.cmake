if(NOT DEFINED SUNNY_SOURCE_DIR OR NOT DEFINED SUNNY_BINARY_DIR)
    message(FATAL_ERROR "architecture rejection test requires source and binary roots")
endif()

function(expect_configure_failure fixture expected_message)
    set(_source "${SUNNY_SOURCE_DIR}/tests/architecture/fixtures/${fixture}")
    set(_binary "${SUNNY_BINARY_DIR}/architecture-fixtures/${fixture}")
    file(REMOVE_RECURSE "${_binary}")
    execute_process(
        COMMAND
            "${CMAKE_COMMAND}"
            -S "${_source}"
            -B "${_binary}"
            "-DSUNNY_ARCHITECTURE_MODULE=${SUNNY_SOURCE_DIR}/cmake/Architecture.cmake"
        RESULT_VARIABLE _result
        OUTPUT_VARIABLE _stdout
        ERROR_VARIABLE _stderr
    )
    if(_result EQUAL 0)
        message(FATAL_ERROR "architecture fixture ${fixture} was incorrectly accepted")
    endif()
    set(_output "${_stdout}\n${_stderr}")
    if(NOT _output MATCHES "${expected_message}")
        message(
            FATAL_ERROR
            "architecture fixture ${fixture} failed for the wrong reason:\n${_output}"
        )
    endif()
endfunction()

expect_configure_failure(forbidden_include "Sunny layer violation")
expect_configure_failure(orphan_source "Sunny source ownership violation")
expect_configure_failure(foreign_source "declares foreign source")
expect_configure_failure(wrong_dependency "Sunny target dependency violation")
expect_configure_failure(source_protocol_drift "Sunny bridge source protocol mismatch")
expect_configure_failure(version_drift "Sunny version authority mismatch")
