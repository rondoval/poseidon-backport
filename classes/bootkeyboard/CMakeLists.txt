# bootkeyboard.class — USB boot-protocol keyboard -> input.device. MUI config GUI.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(bootkeyboard BASETYPE NepHidBase VERSION 4 REVISION 5 PRI 29 GUI DEFINES HAS_LIBOPEN)
