if(NOT PS4)
    return()
endif()

include_guard(GLOBAL)

if(NOT ORBIS_SDK_ROOT)
    message(FATAL_ERROR "PS4 requires cmake/toolchains/orbis.cmake")
endif()

set(_ps4_include "${ORBIS_SDK_ROOT}/target/include")
set(_ps4_include_common "${ORBIS_SDK_ROOT}/target/include_common")
set(_ps4_lib "${ORBIS_SDK_ROOT}/target/lib")
foreach(_path "${_ps4_include}" "${_ps4_include_common}" "${_ps4_lib}")
    if(NOT IS_DIRECTORY "${_path}")
        message(FATAL_ERROR "Required official SDK directory is missing: ${_path}")
    endif()
endforeach()

add_library(eidos_ps4_sdk INTERFACE)
target_include_directories(eidos_ps4_sdk SYSTEM INTERFACE
    "${_ps4_include}"
    "${_ps4_include_common}")
target_link_directories(eidos_ps4_sdk INTERFACE "${_ps4_lib}")

# The baseline and official samples both target x86-64.  Keep the baseline at
# SSE2 and avoid AVX/BMI requirements so one binary covers original/Slim/Pro.
target_compile_options(eidos_ps4_sdk INTERFACE -m64 -msse2 -mno-avx)
target_compile_definitions(eidos_ps4_sdk INTERFACE EIDOS_PLATFORM_PS4=1)

message(STATUS "PS4: official Orbis SDK at ${ORBIS_SDK_ROOT}")
message(STATUS "PS4: baseline x86-64/SSE2; no AVX requirement")
