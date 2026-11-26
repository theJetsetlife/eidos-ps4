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
