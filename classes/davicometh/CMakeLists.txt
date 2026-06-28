# dm9601eth.class — USB Ethernet (Davicom DM9601) -> dm9601eth.device (SANA-II). Dual library +
# MUI GUI. (Class name "dm9601eth", dir "davicometh".) See add_poseidon_class().
add_poseidon_class(dm9601eth BASETYPE NepEthBase VERSION 4 REVISION 3 PRI 48 GUI
    SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers)
