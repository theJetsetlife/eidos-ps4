# Packaging is intentionally separate from compilation.  The official Orbis
# Publishing Tools are a distinct installation and were not present in the
# audited SDK tree.  Configure an explicit executable after installing them.

set(ORBIS_PUBLISH_COMMAND "" CACHE FILEPATH "Official orbis-pub-cmd executable")

function(eidos_ps4_require_packaging_tools)
    if(NOT ORBIS_PUBLISH_COMMAND OR NOT EXISTS "${ORBIS_PUBLISH_COMMAND}")
        message(FATAL_ERROR
            "Official Orbis Publishing Tools are unavailable. Set "
            "ORBIS_PUBLISH_COMMAND to the installed official command; "
            "packaging will not use an unofficial substitute.")
    endif()
endfunction()
