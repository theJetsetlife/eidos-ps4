if(NOT PS4)
    return()
endif()

include_guard(GLOBAL)

function(eidos_ps4_link_video target)
    if(NOT TARGET "${target}")
        message(FATAL_ERROR "eidos_ps4_link_video: unknown target ${target}")
    endif()
    target_link_libraries("${target}" PRIVATE eidos_ps4_sdk
        ScePosix_stub_weak
        SceVideoOut_stub_weak
        SceGnmDriver_stub_weak
        SceSystemService_stub_weak
        SceUserService_stub_weak
        SceGnmx
        SceGnm
        SceGpuAddress)
endfunction()

function(eidos_ps4_make_fself target output_name)
    set(_orbis_bin "${ORBIS_SDK_ROOT}/host_tools/bin/orbis-bin.exe")
    if(NOT EXISTS "${_orbis_bin}")
        message(FATAL_ERROR "Official executable converter is missing: ${_orbis_bin}")
    endif()
    add_custom_command(TARGET "${target}" POST_BUILD
        COMMAND "${_orbis_bin}" "$<TARGET_FILE:${target}>" --oformat=fself
                --outfile "$<TARGET_FILE_DIR:${target}>/${output_name}"
        BYPRODUCTS "$<TARGET_FILE_DIR:${target}>/${output_name}"
        COMMENT "Converting ${target} to an SDK FSELF diagnostic executable"
        VERBATIM)
endfunction()
