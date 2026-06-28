# lan78xx.class — USB Ethernet (Microchip LAN78xx) -> usblan78xx.device (SANA-II). Dual library, no
# GUI. See add_poseidon_class().
add_poseidon_class(lan78xx BASETYPE NepEthBase VERSION 1 REVISION 0 PRI 48
    INCLUDE lan78xx.class.h SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers mui_headers)
