# Official Sony Orbis SDK 12.508 toolchain.
#
# The official compiler driver selects the Orbis target itself.  Official SDK
# samples invoke orbis-clang.exe directly, so this file deliberately does not
# add a desktop Clang --target/sysroot guess.

set(CMAKE_SYSTEM_NAME Orbis)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(PS4 TRUE CACHE BOOL "Build for PlayStation 4 with the official Orbis SDK" FORCE)

if(NOT DEFINED ENV{SCE_ORBIS_SDK_DIR} OR "$ENV{SCE_ORBIS_SDK_DIR}" STREQUAL "")
    message(FATAL_ERROR "SCE_ORBIS_SDK_DIR is not set. Point it at an official Orbis SDK 12.508 installation.")
endif()

file(TO_CMAKE_PATH "$ENV{SCE_ORBIS_SDK_DIR}" ORBIS_SDK_ROOT)
set(ORBIS_SDK_ROOT "${ORBIS_SDK_ROOT}" CACHE PATH "Official Orbis SDK root" FORCE)
set(_orbis_bin "${ORBIS_SDK_ROOT}/host_tools/bin")

set(CMAKE_C_COMPILER "${_orbis_bin}/orbis-clang.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_orbis_bin}/orbis-clang++.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${_orbis_bin}/orbis-clang.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_AR "${_orbis_bin}/orbis-ar.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB "${_orbis_bin}/orbis-ranlib.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_LINKER "${_orbis_bin}/orbis-ld.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_NM "${_orbis_bin}/orbis-nm.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY "${_orbis_bin}/orbis-objcopy.exe" CACHE FILEPATH "" FORCE)
set(CMAKE_OBJDUMP "${_orbis_bin}/orbis-objdump.exe" CACHE FILEPATH "" FORCE)

foreach(_tool CMAKE_C_COMPILER CMAKE_CXX_COMPILER CMAKE_AR CMAKE_LINKER)
    if(NOT EXISTS "${${_tool}}")
        message(FATAL_ERROR "Official SDK tool is missing: ${_tool}=${${_tool}}")
    endif()
endforeach()

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_FIND_ROOT_PATH "${ORBIS_SDK_ROOT}/target")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

get_filename_component(_eidos_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
list(PREPEND CMAKE_MODULE_PATH "${_eidos_root}/cmake/platforms")

unset(_eidos_root)
unset(_orbis_bin)
