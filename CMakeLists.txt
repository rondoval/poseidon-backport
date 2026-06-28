cmake_minimum_required(VERSION 3.14)
project(poseidon_backport VERSION 0.0.0 LANGUAGES C ASM)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/install" CACHE PATH "" FORCE)
endif()

find_program(SFDC_EXECUTABLE NAMES sfdc HINTS /opt/m68k-amigaos/bin REQUIRED)

# MUI SDK headers (not in the NDK) — used by the GUI-bearing classes (boot input
# config dialogs) and Trident. Override with -DMUI_INCLUDE_DIR=... if relocated.
set(MUI_INCLUDE_DIR "/opt/MUI5/SDK/MUI/C/include" CACHE PATH "MUI SDK include dir")
add_library(mui_headers INTERFACE)
target_include_directories(mui_headers INTERFACE ${MUI_INCLUDE_DIR})
# NB: the SDK's <inline/muimaster.h> __inline MUI_NewObject() is broken under bebbo gcc
# -O2. MUI-using sources force-include include/mui_newobject_fix.h,
# which replaces it with a va_list-based version.

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

# --- Debug logging -------------------------------------------------------------
# Switchable output backend (pistorm | serial | off) + verbosity level, shared by every
# component via include/debug.h. Compile defs are applied globally here (before the
# subdirs); per-target -ldebug linking (serial only) is done by psd_debug_finalize() in
# each component. See cmake/PoseidonDebug.cmake.
include(cmake/PoseidonDebug.cmake)
psd_debug_definitions()
message(STATUS "Poseidon: debug backend = ${POSEIDON_DEBUG_BACKEND}, level = ${POSEIDON_DEBUG_LEVEL}")

# --- Components (usbclass first: poseidon + the classes consume its headers) ---
add_subdirectory(usbclass.library)
add_subdirectory(poseidon.library)
add_subdirectory(c)
add_subdirectory(tools)
add_subdirectory(classes/hub)
add_subdirectory(classes/bootmouse)
add_subdirectory(classes/bootkeyboard)
add_subdirectory(classes/massstorage)
add_subdirectory(trident)

include(cmake/Packaging.cmake)
