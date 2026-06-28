# cdceth.class — USB CDC Ethernet (ECM/EEM, the standard) -> usbcdceth.device (SANA-II network
# driver). Dual library (class NepEthBase + embedded usbcdceth.device via dev.c) + CDC encapsulation
# (cdceth_encap.c) + a MUI config GUI; needs the SANA-II headers. See add_poseidon_class().
add_poseidon_class(cdceth BASETYPE NepEthBase VERSION 5 REVISION 0 PRI 48 GUI
    INCLUDE cdceth.class.h
    SOURCES dev.c cdceth_encap.c
    DEFINES HAS_LIBOPEN
    DEPS sana2_headers)
