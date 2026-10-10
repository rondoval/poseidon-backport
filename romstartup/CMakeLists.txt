# romstartup - the Kickstart-ROM startup resident (see usbromstart.c for the shape).
#
# Never installed on the Amiga: it exists only to be linked into a Kickstart image by
# scripts/build-kickstart.py, which takes it from the build tree - or, in the shipped
# archive, from the ROM/ drawer it is packaged into (cmake/Packaging.cmake).

# The default host controller baked into the resident's patchable slot. Changing it
# here is rarely what you want: the ROM image is assembled from whatever modules the
# builder hands to scripts/build-kickstart.py, and that script's --hcd flag rewrites
# the name per image with no rebuild. This is only the fallback for an image built
# without it.
set(ROMSTART_HCD "xhci.device" CACHE STRING
    "Default host-controller device name (build-kickstart.py --hcd overrides per image)")

add_executable(usbromstart
    usbromstart.c
    )
add_dependencies(usbromstart poseidon_sfd_headers usbclass_sfd_headers)

target_compile_options(usbromstart PRIVATE -Os -ffreestanding)

target_compile_definitions(usbromstart PRIVATE
    __NOLIBBASE__
    POSEIDON_BASE_NAME=ps
    ROMSTART_HCD_NAME="${ROMSTART_HCD}")

target_include_directories(usbromstart PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR})

# usbclass_headers: the resident asks massstorage.class directly whether a medium is
# still coming up (UCM_MediaPending, inline LVO macros).
target_link_libraries(usbromstart PRIVATE poseidon_headers usbclass_headers
    -Wl,--start-group -lc -lgcc -Wl,--end-group)

# Linker entry point: a resident module is data to the loader, never a program.
# The entry point is ENTRY() in ldscripts/module.lds, together with the placement of
# .text.entry -- stating it here too would be a second, silently divergent source.
target_link_options(usbromstart PRIVATE
    -nostdlib -nostartfiles -s)

psd_debug_finalize(usbromstart)   # links -ldebug only for the serial backend
psd_module_layout(usbromstart)
