cmake_minimum_required(VERSION 3.14)
project(poseidon.library VERSION 5.3 LANGUAGES C)

# --- sfdc-generated headers (proto/inline/clib) for poseidon + usbclass -------
find_program(SFDC_EXECUTABLE NAMES sfdc HINTS /opt/m68k-amigaos/bin REQUIRED)

set(GEN_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated/include)

function(generate_sfd sfd basename)
    set(_hdrs
        ${GEN_DIR}/clib/${basename}_protos.h
        ${GEN_DIR}/inline/${basename}.h
        ${GEN_DIR}/pragmas/${basename}_pragmas.h
        ${GEN_DIR}/proto/${basename}.h)
    add_custom_command(OUTPUT ${_hdrs}
        COMMAND ${CMAKE_COMMAND}
            -DSFDC_EXECUTABLE=${SFDC_EXECUTABLE}
            -DSFD_FILE=${sfd}
            -DOUTPUT_DIR=${GEN_DIR}
            -DOUTPUT_BASENAME=${basename}
            -P ${CMAKE_SOURCE_DIR}/cmake/GenerateSfdHeaders.cmake
        DEPENDS ${sfd} ${CMAKE_SOURCE_DIR}/cmake/GenerateSfdHeaders.cmake
        VERBATIM)
    set(${basename}_HEADERS ${_hdrs} PARENT_SCOPE)
endfunction()

generate_sfd(${CMAKE_CURRENT_SOURCE_DIR}/poseidon.sfd          poseidon)
generate_sfd(${CMAKE_SOURCE_DIR}/usbclass.library/usbclass.sfd usbclass)
add_custom_target(poseidon_sfd_headers ALL DEPENDS ${poseidon_HEADERS} ${usbclass_HEADERS})

# --- the library --------------------------------------------------------------
add_executable(poseidon_library
    poseidon_main.c
    poseidon.library.c
    numtostr.c
    popo_stub.c            # Phase 1 stub for popo.gui.c (MUI requester); real port later
    poseidon_end.c
)
add_dependencies(poseidon_library poseidon_sfd_headers)

target_compile_options(poseidon_library PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer
    # m68k tag/vararg calls inherently mix pointers and ULONGs (sfdc's vararg
    # inlines marshal them through an array); pointers are ULONG-sized here.
    -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_compile_definitions(poseidon_library PRIVATE __NOLIBBASE__)

target_include_directories(poseidon_library PRIVATE
    ${CMAKE_SOURCE_DIR}/include          # public ABI headers (libraries/, devices/)
    ${CMAKE_CURRENT_SOURCE_DIR}          # poseidon_intern.h, debug.h, ...
    ${CMAKE_CURRENT_SOURCE_DIR}/include  # hidd/usb.h (to be removed later)
    ${GEN_DIR})                          # generated proto/inline/clib

target_link_options(poseidon_library PRIVATE
    -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

# -lc (libnix string fns), -ldebug (KPrintF for PSF_KLOG), -lgcc (intrinsics).
# Grouped so __divsi3 (referenced by debug.lib) resolves against libgcc.
target_link_libraries(poseidon_library PRIVATE
    -Wl,--start-group -lc -ldebug -lgcc -Wl,--end-group)

set_target_properties(poseidon_library PROPERTIES
    OUTPUT_NAME poseidon SUFFIX ".library")
