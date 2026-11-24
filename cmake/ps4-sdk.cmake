if(NOT PS4)
    return()
endif()

include_guard(GLOBAL)

function(eidos_ps4_link_platform target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "eidos_ps4_link_platform: unknown target ${target}")
    endif()
    target_link_libraries("${target}" PRIVATE eidos_platform_ps4)
endfunction()

function(eidos_ps4_make_fself target output_name)
    set(_orbis_bin "${ORBIS_SDK_ROOT}/host_tools/bin/orbis-bin.exe")
    set(_fself_output "${CMAKE_CURRENT_BINARY_DIR}/${output_name}")
    if(NOT EXISTS "${_orbis_bin}")
        message(FATAL_ERROR "Official executable converter is missing: ${_orbis_bin}")
    endif()
    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${_orbis_bin}" "$<TARGET_FILE:${target}>" --oformat=fself
                --outfile "${_fself_output}"
        BYPRODUCTS "${_fself_output}"
        COMMENT "Converting ${target} to an SDK FSELF diagnostic executable"
        VERBATIM)
endfunction()
