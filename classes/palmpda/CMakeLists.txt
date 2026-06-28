# palmpda.class — Palm PDA serial/HotSync -> usbpalm.device. Dual library (NepSerialBase class +
# NepSerDevBase device) + MUI GUI (libbase MUI base, like printer).
add_poseidon_class(palmpda BASETYPE NepSerialBase VERSION 4 REVISION 3 PRI 48 GUI SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers)
