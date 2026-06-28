# serialpl2303.class — USB serial (Prolific PL2303) -> usbmodem.device. No GUI; embeds the
# usbmodem.device (dev.c, MakeLibrary'd in libInit). See add_poseidon_class() in the top CMakeLists.
add_poseidon_class(serialpl2303 BASETYPE NepSerialBase VERSION 4 REVISION 3 PRI 48 SOURCES dev.c)
