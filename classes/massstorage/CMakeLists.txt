# massstorage.class — USB mass-storage -> AmigaDOS volumes. The biggest class: the class library
# PLUS an embedded usbscsi.device (dev.c, MakeLibrary'd at run time) PLUS three USB transports
# (BBB/CBI/UAS) PLUS the mounter (four sources; RDB/MBR/GPT/superfloppy/CD; -DMOUNTER_LOG
# routes its diagnostics to our mounter_log() -> debug backend, -DMOUNTER_TRACE=1
# keeps its per-step dbg() tracing on -- the submodule used to hardcode that, and
# now leaves the choice to the host). MUI config GUI.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(massstorage BASETYPE NepMSBase GUI
    SOURCES massstorage_bulk.c massstorage_cbi.c massstorage_uas.c dev.c
            mounter/mounter.c mounter/mounter_rdb.c mounter/mounter_cd.c mounter/mounter_legacy.c
    DEFINES HAS_LIBOPEN HAS_LIBCLOSE MOUNTER_LOG MOUNTER_TRACE=1)

set_source_files_properties(
    mounter/mounter.c mounter/mounter_rdb.c mounter/mounter_cd.c mounter/mounter_legacy.c
    PROPERTIES
    COMPILE_OPTIONS "-Wall;-Wextra;-Wconversion;-Wsign-conversion;-Wshadow;-Wmissing-prototypes;-Wstrict-prototypes")
