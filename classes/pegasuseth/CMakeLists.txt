# pegasus.class — USB Ethernet (Pegasus / ADMtek) -> usbpegasus.device (SANA-II). Dual library +
# MUI GUI. (Class name "pegasus", dir "pegasuseth".) See add_poseidon_class().
add_poseidon_class(pegasus BASETYPE NepEthBase VERSION 4 REVISION 3 PRI 48 GUI
    SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers)
