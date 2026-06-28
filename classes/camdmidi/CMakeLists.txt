# camdusbmidi.class — USB-MIDI -> CAMD. Single library + MUI config GUI (libbase MUI base,
# simplemidi-style). At bind time it writes a tiny CAMD MIDI driver to DEVS:Midi/; that driver
# opens this class back and calls its two EXTRA library vectors usbCAMDOpenPort@-90 /
# usbCAMDClosePort@-96. camd_vectors.h injects those into the shared skeleton's funcTable.
add_poseidon_class(camdusbmidi BASETYPE NepHidBase VERSION 4 REVISION 5 PRI 39 GUI DEFINES HAS_LIBOPEN)
target_compile_definitions(camdusbmidi_class PRIVATE CLASS_VECTORS_HDR="camd_vectors.h")

# ---------------------------------------------------------------------------------------------
# The embedded CAMD MIDI driver, built from source (camd/poseidonusb.c) instead of a frozen blob.
#
#   camdusbmidi.sfd --(sfdc)--> proto/camdusbmidi.h   (inline usbCAMD* stubs the driver calls)
#   poseidonusb.c --(ld, camddriver.ld)--> poseidonusb   (a CAMD-shaped HUNK exe: the doNotExecute
#                                          entry stub@0, MidiDeviceData@4)
#   poseidonusb --(gen_camddriver.py)--> CAMDDriver.c (ULONG[] + CAMDDriver_NAMEOFFSET)
#   CAMDDriver.c is #included by camdusbmidi.class.c (the #if __mc68000__ path).
# ---------------------------------------------------------------------------------------------

# (a) generate proto/inline/clib headers for the class's exported usbCAMD* vectors
generate_sfd_headers(camdusbmidi CAMD_SFD_GEN)

# (b) build the driver as a CAMD-driver-shaped HUNK exe (entry stub first, MidiDeviceData at +4)
add_executable(camd_poseidonusb camd/poseidonusb.c)
add_dependencies(camd_poseidonusb camdusbmidi_sfd_headers)
target_compile_options(camd_poseidonusb PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)
# EXEC_BASE_NAME satisfies the (dead, never-called) mui_base.h accessor that camdusbmidi.h's
# GUI block drags in — the driver itself uses its own global SysBase for exec.
target_compile_definitions(camd_poseidonusb PRIVATE EXEC_BASE_NAME=SysBase)
target_include_directories(camd_poseidonusb PRIVATE
    ${CAMD_SFD_GEN}                         # generated proto/camdusbmidi.h
    ${CMAKE_SOURCE_DIR}/include             # vendored midi/camddevices.h + aros_compat.h
    ${CMAKE_SOURCE_DIR}/classes             # shared common.h / mui_base.h (via camdusbmidi.h)
    ${CMAKE_CURRENT_SOURCE_DIR})            # camdusbmidi.h
target_link_libraries(camd_poseidonusb PRIVATE mui_headers   # camdusbmidi.h pulls <libraries/mui.h>
    -Wl,--start-group -lc -lamiga -lgcc -Wl,--end-group)
target_link_options(camd_poseidonusb PRIVATE
    -nostdlib -nostartfiles -ffreestanding -s
    -Wl,-T,${CMAKE_CURRENT_SOURCE_DIR}/camd/camddriver.ld -Wl,-e,_doNotExecute)
set_target_properties(camd_poseidonusb PROPERTIES OUTPUT_NAME poseidonusb SUFFIX "")

# (c) codegen the embedded ULONG[] + recompile-safe name offset from the linked driver
set(CAMD_BLOB_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen-blob)
set(CAMD_BLOB ${CAMD_BLOB_DIR}/CAMDDriver.c)
add_custom_command(OUTPUT ${CAMD_BLOB}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CAMD_BLOB_DIR}
    COMMAND python3 ${CMAKE_CURRENT_SOURCE_DIR}/camd/gen_camddriver.py
            $<TARGET_FILE:camd_poseidonusb> ${CAMD_BLOB}
    DEPENDS camd_poseidonusb ${CMAKE_CURRENT_SOURCE_DIR}/camd/gen_camddriver.py
    VERBATIM)
add_custom_target(camd_driver_blob DEPENDS ${CAMD_BLOB})

# (d) the class #include "CAMDDriver.c" now resolves to the generated one (the checked-in copy
#     is gone); make the gen dir win and the class wait for the blob.
add_dependencies(camdusbmidi_class camd_driver_blob)
target_include_directories(camdusbmidi_class BEFORE PRIVATE ${CAMD_BLOB_DIR})
