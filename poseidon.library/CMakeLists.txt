# poseidon.library — the stack core (see planning/03 playbook for the de-AROS recipe).

generate_sfd_headers(poseidon POSEIDON_GEN_DIR)

# Expose poseidon's generated proto/inline/clib so the CLI tools and the
# class drivers can include <proto/poseidon.h>.
add_library(poseidon_headers INTERFACE)
target_include_directories(poseidon_headers INTERFACE ${POSEIDON_GEN_DIR})

add_executable(poseidon_library
    poseidon_main.c
    poseidon.library.c
    numtostr.c
    popo_stub.c            # Phase 1 stub for popo.gui.c (MUI requester); real port later
    poseidon_end.c
)

# Headers must exist before we compile: our own poseidon.sfd + usbclass's (we call usbDoMethodA).
add_dependencies(poseidon_library poseidon_sfd_headers usbclass_sfd_headers)

target_compile_options(poseidon_library PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer
    # m68k tag/vararg calls inherently mix pointers and ULONGs (sfdc's vararg
    # inlines marshal them through an array); pointers are ULONG-sized here.
    -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_compile_definitions(poseidon_library PRIVATE __NOLIBBASE__)

target_include_directories(poseidon_library PRIVATE
    ${CMAKE_SOURCE_DIR}/include          # public ABI headers (libraries/, devices/)
    ${CMAKE_CURRENT_SOURCE_DIR})         # poseidon_intern.h, debug.h, ...

# poseidon_headers + usbclass_headers (INTERFACE) bring the generated-include dirs.
# -lc (libnix string fns), -ldebug (KPrintF for PSF_KLOG), -lgcc (intrinsics);
# grouped so __divsi3 (referenced by debug.lib) resolves against libgcc.
target_link_libraries(poseidon_library PRIVATE
    poseidon_headers usbclass_headers
    -Wl,--start-group -lc -ldebug -lgcc -Wl,--end-group)

target_link_options(poseidon_library PRIVATE
    -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

set_target_properties(poseidon_library PROPERTIES
    OUTPUT_NAME poseidon SUFFIX ".library")
