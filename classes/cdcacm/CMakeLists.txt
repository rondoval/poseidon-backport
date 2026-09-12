# cdcacm.class — USB CDC-ACM serial (modems/OBEX) -> usbmodem.device. No GUI; embeds the
# usbmodem.device (dev.c, MakeLibrary'd in libInit). See add_poseidon_class() in the top CMakeLists.
add_poseidon_class(cdcacm BASETYPE NepSerialBase SOURCES dev.c)
