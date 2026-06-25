cmake_minimum_required(VERSION 3.14)
project(poseidon_backport VERSION 0.0.0 LANGUAGES C ASM)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

find_program(SFDC_EXECUTABLE NAMES sfdc HINTS /opt/m68k-amigaos/bin REQUIRED)

# Shared helper: generate clib/inline/pragmas/proto headers from <basename>.sfd
# (in the calling component's dir) into <bindir>/generated/include. Creates the
# custom target <basename>_sfd_headers and returns the generated-include dir.
function(generate_sfd_headers basename out_gen_dir)
    set(gen ${CMAKE_CURRENT_BINARY_DIR}/generated/include)
    set(sfd ${CMAKE_CURRENT_SOURCE_DIR}/${basename}.sfd)
    set(hdrs
        ${gen}/clib/${basename}_protos.h
        ${gen}/inline/${basename}.h
        ${gen}/pragmas/${basename}_pragmas.h
        ${gen}/proto/${basename}.h)
    add_custom_command(OUTPUT ${hdrs}
        COMMAND ${CMAKE_COMMAND}
            -DSFDC_EXECUTABLE=${SFDC_EXECUTABLE}
            -DSFD_FILE=${sfd}
            -DOUTPUT_DIR=${gen}
            -DOUTPUT_BASENAME=${basename}
            -P ${CMAKE_SOURCE_DIR}/cmake/GenerateSfdHeaders.cmake
        DEPENDS ${sfd} ${CMAKE_SOURCE_DIR}/cmake/GenerateSfdHeaders.cmake
        VERBATIM)
    add_custom_target(${basename}_sfd_headers DEPENDS ${hdrs})
    set(${out_gen_dir} ${gen} PARENT_SCOPE)
endfunction()

# NOTE: emu68-common (shared debug backend / minlist / etc.) is DEFERRED — added
# as a submodule only if/when a component needs it.

# --- Components (usbclass first: poseidon + the classes consume its headers) ---
add_subdirectory(usbclass.library)     # Phase 2 — ABI headers only (no binary)
add_subdirectory(poseidon.library)     # Phase 1
# add_subdirectory(classes/hub)        # Phase 4 ...
