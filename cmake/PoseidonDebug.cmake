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
        # debug.lib is a single object, so pulling KPutChar also drags KGetNum -> __divsi3,
        # which on this toolchain lives in libnix's libc (not libgcc).  This link group is
        # appended after the target's own one, by which point -lc has already been scanned,
        # so -ldebug has to bring libc along or the reference goes unsatisfied.  (That is
        # what the hand-written weak __divsi3 used to paper over.)  Bare "debug" is a
        # reserved target_link_libraries keyword, hence the flag form.
        target_link_libraries(${target} PRIVATE
            -Wl,--start-group -ldebug -lc -Wl,--end-group)
    endif()
endfunction()
