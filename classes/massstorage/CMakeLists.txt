# massstorage.class — USB mass-storage -> AmigaDOS volumes. The biggest class:
# the class library PLUS an embedded usbscsi.device (dev.c, MakeLibrary'd at run
# time) PLUS three USB transports (BBB/CBI/UAS) PLUS the A4091 mounter submodule
# (RDB/MBR/GPT). MUI config GUI. Shared class skeleton; freestanding library.

add_executable(massstorage_class
    massstorage.class.c
    massstorage_bulk.c
    massstorage_cbi.c
    massstorage_uas.c
    dev.c
    ${CMAKE_SOURCE_DIR}/classes/debug.c
    mounter/mounter.c
    ${CMAKE_SOURCE_DIR}/classes/class_main.c
    ${CMAKE_SOURCE_DIR}/classes/class_end.c)

add_dependencies(massstorage_class poseidon_sfd_headers)

target_compile_options(massstorage_class PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_compile_definitions(massstorage_class PRIVATE
    __NOLIBBASE__
    POSEIDON_BASE_NAME=ps
    CLASS_NAME="massstorage.class"
    CLASS_INCLUDE="massstorage.h"
    CLASS_BASETYPE_NAME=NepMSBase
    CLASS_VERSION=5
    CLASS_REVISION=0
    CLASS_PRI=39
    HAS_LIBOPEN
    HAS_LIBCLOSE
    DISKLABELS)        # mounter: enable MBR + GPT (structs in mounter/legacy.h)

target_include_directories(massstorage_class PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/classes         # shared common.h / debug.h
    ${CMAKE_CURRENT_SOURCE_DIR})        # the class headers

target_link_libraries(massstorage_class PRIVATE
    poseidon_headers
    mui_headers
    -Wl,--start-group -lc -lamiga -ldebug -lgcc -Wl,--end-group)  # -lamiga: DoMethod + Create*

target_link_options(massstorage_class PRIVATE
    -ffreestanding -nostdlib -nostartfiles -s -Wl,-e,_doNotExecute)

set_target_properties(massstorage_class PROPERTIES OUTPUT_NAME massstorage SUFFIX ".class")
