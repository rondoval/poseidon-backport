# PoseidonRomCheck.cmake — POST_BUILD guard for the modules that go into a
# Kickstart image.
#
# The ROM is mapped read-only, so a ROM module must contain no writable data
# at all: any non-empty .data or .bss fails the build.  poseidon.library and
# the class drivers already satisfy this by construction — exec base from
# absolute $4, all mutable state in the allocated library base — but nothing
# enforced it before, so a stray writable global could only be caught by
# a cold-boot crash.
#
# The serial debug backend is exempt: debug.lib carries a 4-byte writable
# _SysBase, so a serial build of a ROM module can never be clean.  That build is
# a debugging aid, not a shippable ROM — the pistorm and off backends, which are
# what a ROM is actually built from, stay checked.
#
# Usage:
#   include(cmake/PoseidonRomCheck.cmake)
#   psd_rom_check(<target>)

set(_PSD_ROM_CHECK_SCRIPT "${CMAKE_CURRENT_LIST_DIR}/PoseidonRomCheckScript.cmake"
    CACHE INTERNAL "psd_rom_check helper script")

function(psd_rom_check target)
    if(POSEIDON_DEBUG_BACKEND STREQUAL "serial")
        return()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND}
            -DOBJDUMP=${CMAKE_OBJDUMP}
            -DBINARY=$<TARGET_FILE:${target}>
            -P ${_PSD_ROM_CHECK_SCRIPT}
        COMMENT "ROM check: ${target} must contain no writable data sections"
        VERBATIM
    )
endfunction()
