# PoseidonModuleLayoutCheckScript.cmake
# Helper for psd_module_layout(): assert the linked module really starts with the
# do-not-execute stub.  Invoked as: cmake -DBINARY=... -DEXPECT=... -P <this>.
#
# ldscripts/module.lds places .text.entry first, but only if something is IN it -- a new
# module whose stub is missing __attribute__((section(".text.entry"))) links happily with
# whatever the compiler emitted first sitting at offset 0.  That is exactly the bug this
# contract exists to prevent, so check the bytes that ship.
#
# AmigaOS HUNK header: HUNK_HEADER(4) + reslist terminator(4) + table_size(4) + first(4)
# + last(4) + one size longword per hunk, then HUNK_CODE(4) + length(4).  So the first
# hunk's payload begins at 28 + 4 * numhunks.

file(READ "${BINARY}" _n_hex OFFSET 8 LIMIT 4 HEX)
math(EXPR _payload "28 + 4 * 0x${_n_hex}")
file(READ "${BINARY}" _got OFFSET ${_payload} LIMIT 4 HEX)
if(NOT _got STREQUAL "${EXPECT}")
    message(FATAL_ERROR
        "${BINARY}: module starts with ${_got}, expected ${EXPECT} "
        "(moveq #-1,d0; rts).\n"
        "  LoadSeg() runs offset 0, so the do-not-execute stub must be there.  Give it\n"
        "  __attribute__((used, section(\".text.entry\"))) -- see PoseidonModuleLayout.cmake.")
endif()
