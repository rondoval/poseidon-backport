# serialcp210x.class — USB serial (SiLabs CP210x) -> usbmodem.device. No GUI; embeds the
# usbmodem.device (dev.c, MakeLibrary'd in libInit). See add_poseidon_class() in the top CMakeLists.
add_poseidon_class(serialcp210x BASETYPE NepSerialBase VERSION 4 REVISION 3 PRI 48 SOURCES dev.c)
