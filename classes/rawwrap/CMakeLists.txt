# rawwrap.class — raw USB endpoint passthrough -> usbraw.device. Dual library (class + embedded
# device via dev.c) + MUI GUI (per-instance MUI base).
add_poseidon_class(rawwrap BASETYPE NepRawWrapBase VERSION 4 REVISION 3 PRI 48 GUI SOURCES dev.c DEFINES HAS_LIBOPEN)
