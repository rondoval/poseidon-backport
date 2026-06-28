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

# Shared helper: define a USB class driver — a freestanding .class library built from the
# shared classes/class_main.c skeleton + <name>.class.c. Call from the class's dir:
#
#   add_poseidon_class(bootmouse BASETYPE NepHidBase VERSION 4 REVISION 4 PRI 29 GUI DEFINES HAS_LIBOPEN)
#
# Options:  GUI       links mui_headers + -lamiga (config dialog: MUI + BOOPSI DoMethod)
#           AMIGALIB  links -lamiga without MUI (a non-GUI class that still uses amiga.lib stubs)
# One-val:  BASETYPE VERSION REVISION PRI (required); INCLUDE (default "<name>.h")
# Multi:    SOURCES   extra .c beyond <name>.class.c + the skeleton
#           DEFINES   extra -D (HAS_LIBOPEN, HAS_LIBCLOSE, DISKLABELS, …)
#           DEPS      extra link targets (e.g. sana2_headers for the ethernet family)
function(add_poseidon_class name)
    cmake_parse_arguments(PC "GUI;AMIGALIB" "BASETYPE;VERSION;REVISION;PRI;INCLUDE" "SOURCES;DEFINES;DEPS" ${ARGN})
    if(NOT PC_INCLUDE)
        set(PC_INCLUDE "${name}.h")
    endif()
    set(tgt ${name}_class)

    add_executable(${tgt}
        ${name}.class.c ${PC_SOURCES}
        ${CMAKE_SOURCE_DIR}/classes/class_main.c
        ${CMAKE_SOURCE_DIR}/classes/class_end.c)
    add_dependencies(${tgt} poseidon_sfd_headers)

    target_compile_options(${tgt} PRIVATE
        -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
        -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

    target_compile_definitions(${tgt} PRIVATE
        __NOLIBBASE__
        POSEIDON_BASE_NAME=ps
        CLASS_NAME="${name}.class"             # MOD_NAME_STRING/VERSION_* derive from CLASS_* (common.h)
        CLASS_INCLUDE="${PC_INCLUDE}"
        CLASS_BASETYPE_NAME=${PC_BASETYPE}
        CLASS_VERSION=${PC_VERSION}
        CLASS_REVISION=${PC_REVISION}
        CLASS_PRI=${PC_PRI}
        ${PC_DEFINES})

    target_include_directories(${tgt} PRIVATE
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/classes            # shared common.h / debug.h
        ${CMAKE_CURRENT_SOURCE_DIR})

    set(_amiga "")
    if(PC_GUI OR PC_AMIGALIB)
        set(_amiga -lamiga)                    # BOOPSI DoMethod / NewObject stubs
    endif()
    set(_libs poseidon_headers ${PC_DEPS})
    if(PC_GUI)
        list(APPEND _libs mui_headers)
    endif()
    target_link_libraries(${tgt} PRIVATE ${_libs}
        -Wl,--start-group -lc ${_amiga} -lgcc -Wl,--end-group)

    target_link_options(${tgt} PRIVATE
        -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

    set_target_properties(${tgt} PROPERTIES OUTPUT_NAME ${name} SUFFIX ".class")
    psd_debug_finalize(${tgt})                 # links -ldebug only for the serial backend
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
add_subdirectory(classes/cdcacm)
add_subdirectory(classes/serialpl2303)
add_subdirectory(classes/serialcp210x)
add_subdirectory(classes/printer)
add_subdirectory(trident)

include(cmake/Packaging.cmake)
