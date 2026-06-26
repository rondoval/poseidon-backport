# hub.class — root + external hub driver. Full genmodule library (the shared
# classes/class_main.c skeleton + the de-AROS'd hub.class.c). Freestanding library
# (its own bases via __NOLIBBASE__); output name hub.class.

add_executable(hub_class
    hub.class.c
    ${CMAKE_SOURCE_DIR}/classes/debug.c
    ${CMAKE_SOURCE_DIR}/classes/class_main.c
    ${CMAKE_SOURCE_DIR}/classes/class_end.c)

add_dependencies(hub_class poseidon_sfd_headers)

target_compile_options(hub_class PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_compile_definitions(hub_class PRIVATE
    __NOLIBBASE__
    POSEIDON_BASE_NAME=ps            # class calls psd* via `ps` (#define ps nch->nch_Base + locals)
    CLASS_NAME="hub.class"           # MOD_NAME_STRING/VERSION_* derive from CLASS_* (classes/common.h)
    CLASS_INCLUDE="hub.h"
    CLASS_BASETYPE_NAME=NepHubBase
    CLASS_VERSION=4
    CLASS_REVISION=3
    CLASS_PRI=47)                    # hub has no libOpen hook (no HAS_LIBOPEN)

target_include_directories(hub_class PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/classes        # shared common.h / debug.h
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(hub_class PRIVATE
    poseidon_headers
    -Wl,--start-group -lc -ldebug -lgcc -Wl,--end-group)

target_link_options(hub_class PRIVATE
    -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

set_target_properties(hub_class PROPERTIES OUTPUT_NAME hub SUFFIX ".class")
