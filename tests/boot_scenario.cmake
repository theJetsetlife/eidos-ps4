#
# tests/boot_scenario.cmake -- run the boot through one scripted scenario and
# judge it (a regression guard for a path the game has to get through).
#
#   cmake -DBOOT=<eidos_boot> -DDATA=<BW_DATA_ROOT> -DFRAMES=<n>
#         [-DENV=<NAME=VALUE;...>] [-DPRESS_FROM=<f> -DPRESS_TO=<f>
#          -DPRESS_EVERY=<n> -DPRESS_KEY=<key>] [-DRESIDENT_HAS=<id>]
#         [-DAUDIO_AFTER=<seconds> -DAUDIO_MIN_PEAK=<n> -DPYTHON=<python>]
#         [-DWATCHDOG=<seconds>] -P boot_scenario.cmake
#
# Without the player's game data (DATA/a and DATA/../arm9_dec.bin) the test
# prints SKIPPED.  It passes when the run reaches all FRAMES frames
# ("eidos_boot: ran after FRAMES frames") with no crash, panic or watchdog,
# and, when asked, the overlays resident at the end include RESIDENT_HAS and
# the game's audio is not silent after AUDIO_AFTER seconds of output
# (tests/wav_level.py measures the recording).  The save folder is a fresh
# one under the build tree, so no player's save is read or written.
#
if(NOT DEFINED WATCHDOG)
    set(WATCHDOG 120)
endif()
if(NOT EXISTS "${DATA}/a" OR NOT EXISTS "${DATA}/../arm9_dec.bin")
    message("SKIPPED: no game data at ${DATA} (or no arm9_dec.bin beside it)")
    return()
endif()
if(NOT DEFINED NAME)
    set(NAME scenario)
endif()
set(work "${CMAKE_CURRENT_BINARY_DIR}/boot_scenario_${NAME}")
file(REMOVE_RECURSE "${work}")
file(MAKE_DIRECTORY "${work}/save")

set(args --data-root "${DATA}" --frames ${FRAMES} --watchdog ${WATCHDOG})
if(DEFINED PRESS_FROM)
    if(NOT DEFINED PRESS_KEY)
        set(PRESS_KEY A)
    endif()
    foreach(f RANGE ${PRESS_FROM} ${PRESS_TO} ${PRESS_EVERY})
        list(APPEND args --press "${PRESS_KEY}@${f}:4")
    endforeach()
endif()

set(env "BW_SAVE_DIR=${work}/save" "EIDOS_USER_DIR=${work}")
if(DEFINED ENV)
    list(APPEND env ${ENV})
endif()
if(DEFINED AUDIO_AFTER)
    list(APPEND env "EIDOS_AUDIO_WAV=${work}/audio.wav")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env ${env} "${BOOT}" ${args}
    RESULT_VARIABLE rc OUTPUT_VARIABLE out ERROR_VARIABLE err)
set(all "${out}${err}")

if(NOT all MATCHES "eidos_boot: ran after ${FRAMES} frames")
    message(FATAL_ERROR "the run did not reach ${FRAMES} frames (exit ${rc}):\n${all}")
endif()
if(all MATCHES "(CRASH:|PANIC|WATCHDOG:|AddressSanitizer)")
    message(FATAL_ERROR "the run reported a fault:\n${all}")
endif()
if(DEFINED RESIDENT_HAS)
    string(REGEX MATCH "resident overlays:[ 0-9]*" res "${all}")
    if(NOT res MATCHES " ${RESIDENT_HAS}( |$)")
        message(FATAL_ERROR "overlay ${RESIDENT_HAS} is not resident at the end (${res}):\n${all}")
    endif()
endif()
if(DEFINED AUDIO_AFTER)
    execute_process(
        COMMAND "${PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/wav_level.py"
                "${work}/audio.wav" --after ${AUDIO_AFTER} --min-peak ${AUDIO_MIN_PEAK}
        RESULT_VARIABLE wrc OUTPUT_VARIABLE wout ERROR_VARIABLE werr)
    if(NOT wrc EQUAL 0)
        message(FATAL_ERROR "audio check failed:\n${wout}${werr}")
    endif()
    message("${wout}")
endif()
message("PASS: ${NAME}")
