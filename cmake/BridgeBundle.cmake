# Package the exact private Python adapter that native consumers expect.
# This identity describes source compatibility, not a qualified Live runtime.
function(sunny_prepare_bridge_bundle source_root bundle_root identity_output)
    foreach(_required IN ITEMS
            __init__.py build_identity.py diagnostics.py handler.py managed.py
            managed_capacity.py managed_devices.py managed_recovery.py
            native_units.py server.py surface.py
            bridge_contract.json)
        if(NOT EXISTS "${source_root}/${_required}" OR IS_DIRECTORY "${source_root}/${_required}")
            message(FATAL_ERROR
                "Sunny Remote Script is incomplete: missing ${source_root}/${_required}. Restore the matching source bundle.")
        endif()
    endforeach()

    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS LIST_DIRECTORIES false
        RELATIVE "${source_root}" "${source_root}/*.py")
    list(SORT _sources)
    set(_manifest "sunny-remote-script-v1\n")
    foreach(_source IN LISTS _sources)
        if(NOT _source MATCHES "^([A-Za-z0-9_]+/)*[A-Za-z0-9_]+\\.py$")
            message(FATAL_ERROR "Unsupported Sunny Remote Script source path: ${_source}")
        endif()
        file(SHA256 "${source_root}/${_source}" _file_digest)
        string(APPEND _manifest "${_source}\n${_file_digest}\n")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${source_root}/${_source}")
    endforeach()
    string(SHA256 _identity "${_manifest}")

    # Reconfigure removes retired modules and cached bytecode from the generated bundle.
    file(REMOVE_RECURSE "${bundle_root}")
    file(MAKE_DIRECTORY "${bundle_root}")
    foreach(_source IN LISTS _sources)
        get_filename_component(_source_directory "${_source}" DIRECTORY)
        file(MAKE_DIRECTORY "${bundle_root}/${_source_directory}")
        configure_file("${source_root}/${_source}" "${bundle_root}/${_source}" COPYONLY)
    endforeach()
    configure_file("${source_root}/bridge_contract.json"
        "${bundle_root}/bridge_contract.json" COPYONLY)
    file(WRITE "${bundle_root}/source.sha256" "${_identity}\n")
    set(${identity_output} "${_identity}" PARENT_SCOPE)
endfunction()
