# bootkeyboard.class — USB boot-protocol keyboard -> input.device. Has a MUI config
# GUI (needs the MUI SDK headers). Shared class skeleton; freestanding library.

add_executable(bootkeyboard_class
    bootkeyboard.class.c
    ${CMAKE_SOURCE_DIR}/classes/debug.c
    ${CMAKE_SOURCE_DIR}/classes/class_main.c
    ${CMAKE_SOURCE_DIR}/classes/class_end.c)

add_dependencies(bootkeyboard_class poseidon_sfd_headers)

target_compile_options(bootkeyboard_class PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_compile_definitions(bootkeyboard_class PRIVATE
    __NOLIBBASE__
    POSEIDON_BASE_NAME=ps
    CLASS_NAME="bootkeyboard.class"
    CLASS_INCLUDE="bootkeyboard.h"
    CLASS_BASETYPE_NAME=NepHidBase
    CLASS_VERSION=4
    CLASS_REVISION=5
    CLASS_PRI=29
    HAS_LIBOPEN)

target_include_directories(bootkeyboard_class PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/classes        # shared common.h / debug.h
    ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(bootkeyboard_class PRIVATE
    poseidon_headers
    mui_headers
    -Wl,--start-group -lc -lamiga -ldebug -lgcc -Wl,--end-group)  # -lamiga: BOOPSI DoMethod

target_link_options(bootkeyboard_class PRIVATE
    -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

set_target_properties(bootkeyboard_class PROPERTIES OUTPUT_NAME bootkeyboard SUFFIX ".class")
