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
    set(_embedded "${_eidos_pssl_output_dir}/${_name}.h")
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
    string(REPLACE "." "_" _symbol "eidos_ps4_${_name}_sb")
    add_custom_command(OUTPUT "${_embedded}"
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ps4/embed_shader.py"
            --input "${_output}"
            --output "${_embedded}"
            --symbol "${_symbol}"
        DEPENDS "${_output}" "${CMAKE_CURRENT_SOURCE_DIR}/tools/ps4/embed_shader.py"
        COMMENT "Embedding ${source} PSSL binary in the PS4 application"
        VERBATIM)
    set_property(GLOBAL APPEND PROPERTY EIDOS_PS4_SHADER_OUTPUTS "${_output}")
    set_property(GLOBAL APPEND PROPERTY EIDOS_PS4_SHADER_HEADER_OUTPUTS "${_embedded}")
endfunction()

eidos_ps4_compile_shader(fullscreen_vs.pssl sce_vs_vs_orbis)
eidos_ps4_compile_shader(fullscreen_ps.pssl sce_ps_orbis)
eidos_ps4_compile_shader(diagnostic_colorbars_ps.pssl sce_ps_orbis)
eidos_ps4_compile_shader(diagnostic_ds_composite_ps.pssl sce_ps_orbis)
get_property(_eidos_pssl_outputs GLOBAL PROPERTY EIDOS_PS4_SHADER_OUTPUTS)
add_custom_target(eidos_ps4_shaders ALL DEPENDS ${_eidos_pssl_outputs})
get_property(_eidos_pssl_header_outputs GLOBAL PROPERTY EIDOS_PS4_SHADER_HEADER_OUTPUTS)
add_custom_target(eidos_ps4_shader_headers ALL DEPENDS ${_eidos_pssl_header_outputs})
add_dependencies(eidos_platform_ps4 eidos_ps4_shader_headers)
target_sources(eidos_platform_ps4 PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/platform/ps4/shader_blobs_ps4.cpp")
target_include_directories(eidos_platform_ps4 PRIVATE "${_eidos_pssl_output_dir}")

unset(_eidos_psslc)
unset(_eidos_pssl_sources)
unset(_eidos_pssl_script)
unset(_eidos_pssl_output_dir)
unset(_eidos_pssl_cache_dir)
unset(_eidos_pssl_include_files)
unset(_eidos_pssl_outputs)
unset(_eidos_pssl_header_outputs)
