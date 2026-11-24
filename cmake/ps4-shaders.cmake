if(NOT PS4)
    return()
endif()

include_guard(GLOBAL)

find_package(Python3 COMPONENTS Interpreter REQUIRED)
set(_eidos_psslc "${ORBIS_SDK_ROOT}/host_tools/bin/orbis-wave-psslc.exe")
if(NOT EXISTS "${_eidos_psslc}")
    message(FATAL_ERROR "Official PSSL compiler is missing: ${_eidos_psslc}")
endif()

set(_eidos_pssl_sources "${CMAKE_CURRENT_SOURCE_DIR}/shaders/ps4")
set(_eidos_pssl_script "${CMAKE_CURRENT_SOURCE_DIR}/tools/ps4/compile_shaders.py")
set(_eidos_pssl_output_dir "${CMAKE_CURRENT_BINARY_DIR}/ps4-shaders")
set(_eidos_pssl_cache_dir "${CMAKE_CURRENT_BINARY_DIR}/pssl-cache")
file(GLOB_RECURSE _eidos_pssl_include_files CONFIGURE_DEPENDS
    "${_eidos_pssl_sources}/*.pssl"
    "${_eidos_pssl_sources}/*.h"
    "${_eidos_pssl_sources}/*.hlsli"
    "${_eidos_pssl_sources}/*.inc")

function(eidos_ps4_compile_shader source profile)
    get_filename_component(_name "${source}" NAME_WE)
    set(_input "${_eidos_pssl_sources}/${source}")
    set(_output "${_eidos_pssl_output_dir}/${_name}.sb")
    add_custom_command(OUTPUT "${_output}"
        COMMAND "${Python3_EXECUTABLE}" "${_eidos_pssl_script}"
            --compiler "${_eidos_psslc}"
            --source "${_input}"
            --output "${_output}"
            --profile "${profile}"
            --cache-dir "${_eidos_pssl_cache_dir}"
            --include-dir "${_eidos_pssl_sources}"
        DEPENDS "${_input}" "${_eidos_pssl_script}" "${_eidos_psslc}"
            ${_eidos_pssl_include_files}
        COMMENT "Compiling ${source} with the official Orbis PSSL compiler"
        VERBATIM)
    set_property(GLOBAL APPEND PROPERTY EIDOS_PS4_SHADER_OUTPUTS "${_output}")
endfunction()

eidos_ps4_compile_shader(fullscreen_vs.pssl sce_vs_vs_lvp)
eidos_ps4_compile_shader(fullscreen_ps.pssl sce_ps_lvp)
get_property(_eidos_pssl_outputs GLOBAL PROPERTY EIDOS_PS4_SHADER_OUTPUTS)
add_custom_target(eidos_ps4_shaders ALL DEPENDS ${_eidos_pssl_outputs})

unset(_eidos_psslc)
unset(_eidos_pssl_sources)
unset(_eidos_pssl_script)
unset(_eidos_pssl_output_dir)
unset(_eidos_pssl_cache_dir)
unset(_eidos_pssl_include_files)
unset(_eidos_pssl_outputs)
