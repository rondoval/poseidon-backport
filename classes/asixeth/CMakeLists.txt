# asixeth.class — USB Ethernet (ASIX AX887xx chipsets) -> usbasixeth.device (SANA-II).
# Dual library + MUI GUI; needs the SANA-II headers. See add_poseidon_class().
add_poseidon_class(asixeth BASETYPE NepEthBase VERSION 4 REVISION 3 PRI 48 GUI
    SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers)
