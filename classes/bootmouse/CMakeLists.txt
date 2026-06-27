# bootmouse.class — USB boot-protocol mouse -> input.device. Has a MUI config GUI
# (needs the MUI SDK headers). Shared class skeleton; freestanding library.

add_executable(bootmouse_class
    bootmouse.class.c
    ${CMAKE_SOURCE_DIR}/classes/class_main.c
    ${CMAKE_SOURCE_DIR}/classes/class_end.c)

add_dependencies(bootmouse_class poseidon_sfd_headers)

target_compile_options(bootmouse_class PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_compile_definitions(bootmouse_class PRIVATE
    __NOLIBBASE__
    POSEIDON_BASE_NAME=ps
    CLASS_NAME="bootmouse.class"
    CLASS_INCLUDE="bootmouse.h"
    CLASS_BASETYPE_NAME=NepHidBase
    CLASS_VERSION=4
    CLASS_REVISION=4
    CLASS_PRI=29
    HAS_LIBOPEN)

target_include_directories(bootmouse_class PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/classes        # shared common.h / debug.h
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(bootmouse_class PRIVATE
    poseidon_headers
    mui_headers
    -Wl,--start-group -lc -lamiga -lgcc -Wl,--end-group)  # -lamiga: BOOPSI DoMethod

target_link_options(bootmouse_class PRIVATE
    -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

set_target_properties(bootmouse_class PROPERTIES OUTPUT_NAME bootmouse SUFFIX ".class")
psd_debug_finalize(bootmouse_class)   # links -ldebug only for the serial backend
