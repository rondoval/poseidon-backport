# PoseidonDebug.cmake — stack-wide debug-output backend selection.
#
#   pistorm (default) - RawDoFmt -> magic 0xdeadbeef trap (Emu68/PiStorm). No debug.lib.
#   serial            - RawDoFmt -> debug.lib KPutChar -> serial @ 9600. Links libdebug.a.
#   off               - DEBUG undefined; all logging (include/debug.h) compiled out.
#
# Verbosity is a separate knob, POSEIDON_DEBUG_LEVEL -> -DDB_LEVEL=<n> (1 = show all).
#
# Usage:
#   include(cmake/PoseidonDebug.cmake)
#   psd_debug_definitions()                 # once, before add_subdirectory()
#   psd_debug_finalize(<target>)            # per built target, after it is defined

set(POSEIDON_DEBUG_BACKEND "pistorm" CACHE STRING "Debug output backend: pistorm | serial | off")
set_property(CACHE POSEIDON_DEBUG_BACKEND PROPERTY STRINGS pistorm serial off)

if(NOT POSEIDON_DEBUG_BACKEND MATCHES "^(pistorm|serial|off)$")
    message(FATAL_ERROR
        "POSEIDON_DEBUG_BACKEND must be pistorm, serial or off (got '${POSEIDON_DEBUG_BACKEND}')")
endif()

set(POSEIDON_DEBUG_LEVEL 1 CACHE STRING
    "Min message priority emitted (KPRINTF level >= this): 1 = all/verbose, higher = quieter")

# Weak __divsi3 helper, compiled into any target that links libdebug.a. debug.lib is a
# single object: pulling KPutChar also drags KGetNum -> __divsi3, and -ldebug is appended
# after the components' --start-group, so a real object (not a trailing library) must
# satisfy __divsi3. Weak so libc's strong copy wins for the hosted programs.
set(POSEIDON_DEBUG_SERIAL_GLUE "${CMAKE_CURRENT_LIST_DIR}/poseidon_debug_serial_glue.c"
    CACHE INTERNAL "Poseidon serial-debug __divsi3 glue source")

# Apply the backend's compile definitions to the current directory and below.
# Call once at the top level before the add_subdirectory() calls.
macro(psd_debug_definitions)
    if(POSEIDON_DEBUG_BACKEND STREQUAL "pistorm")
        add_compile_definitions(DEBUG DB_LEVEL=${POSEIDON_DEBUG_LEVEL})
    elseif(POSEIDON_DEBUG_BACKEND STREQUAL "serial")
        add_compile_definitions(DEBUG DEBUG_SERIAL DB_LEVEL=${POSEIDON_DEBUG_LEVEL})
    endif()
    # "off": no defines -> include/debug.h compiles the logging macros out.
endmacro()

# Link libdebug.a (+ the __divsi3 glue) iff the serial sink is actually used, i.e. the
# serial backend. pistorm/off reference no debug.lib symbol. Uniform across all targets.
function(psd_debug_finalize target)
    if(POSEIDON_DEBUG_BACKEND STREQUAL "serial")
        # bare "debug" is a reserved target_link_libraries keyword -> pass as a flag.
        target_link_libraries(${target} PRIVATE -ldebug)
        target_sources(${target} PRIVATE ${POSEIDON_DEBUG_SERIAL_GLUE})
    endif()
endfunction()
