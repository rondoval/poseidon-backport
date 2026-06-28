# bootmouse.class — USB boot-protocol mouse -> input.device. MUI config GUI.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(bootmouse BASETYPE NepHidBase VERSION 4 REVISION 4 PRI 29 GUI DEFINES HAS_LIBOPEN)
