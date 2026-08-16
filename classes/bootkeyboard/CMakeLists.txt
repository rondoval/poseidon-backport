# bootkeyboard.class — USB boot-protocol keyboard -> input.device. MUI config GUI.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(bootkeyboard BASETYPE NepHidBase PRI 29 GUI DEFINES HAS_LIBOPEN)
