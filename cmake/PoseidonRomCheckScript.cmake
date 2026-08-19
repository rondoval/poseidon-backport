# Helper for psd_rom_check(): fail if ${BINARY} has a non-empty .data or .bss
# section.  Invoked as: cmake -DOBJDUMP=... -DBINARY=... -P <this>.

execute_process(
    COMMAND ${OBJDUMP} -h ${BINARY}
    OUTPUT_VARIABLE _sections
    RESULT_VARIABLE _res
)
if(NOT _res EQUAL 0)
    message(FATAL_ERROR "psd_rom_check: ${OBJDUMP} -h ${BINARY} failed")
endif()

# Section header lines look like:  "  1 .data    00000008  ..."
string(REGEX MATCHALL "\\.(data|bss)[ \t]+[0-9a-fA-F]+" _hits "${_sections}")
foreach(_hit IN LISTS _hits)
    if(NOT _hit MATCHES "[ \t]0+$")
        message(FATAL_ERROR
            "${BINARY}: contains a non-empty writable section (${_hit}) — not "
            "ROM-able. const-ify it, or move the state into an allocated struct.")
    endif()
endforeach()
