# arosx.class — XInput gamepads -> input.device. Dual library: the class (AROSXClassBase) embeds a
# full arosx.library (AROSXBase, the XInput event API) MakeLibrary'd in libInit; + a MUI config GUI
# that OpenLibrary's arosx.library and calls it via the generated proto/arosx.h.
# arosx.library.c's lib* hooks were renamed ax* to avoid colliding with the class skeleton's.
# arosx_test.c (standalone test program) is intentionally not built.
generate_sfd_headers(arosx AROSX_GEN)
add_poseidon_class(arosx BASETYPE AROSXClassBase VERSION 4 REVISION 3 PRI 39 GUI
    SOURCES arosx.library.c arosx.class.config.gui.c DEFINES HAS_LIBOPEN)
add_dependencies(arosx_class arosx_sfd_headers)
target_include_directories(arosx_class PRIVATE ${AROSX_GEN})
