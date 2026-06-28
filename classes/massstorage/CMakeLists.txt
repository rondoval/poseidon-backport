# massstorage.class — USB mass-storage -> AmigaDOS volumes. The biggest class: the class library
# PLUS an embedded usbscsi.device (dev.c, MakeLibrary'd at run time) PLUS three USB transports
# (BBB/CBI/UAS) PLUS the A4091 mounter submodule (RDB/MBR/GPT, -DDISKLABELS). MUI config GUI.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(massstorage BASETYPE NepMSBase VERSION 5 REVISION 0 PRI 39 GUI
    SOURCES massstorage_bulk.c massstorage_cbi.c massstorage_uas.c dev.c mounter/mounter.c
    DEFINES HAS_LIBOPEN HAS_LIBCLOSE DISKLABELS)
