# moschipeth.class — USB Ethernet (MosChip MCS78xx) -> usbmoschipeth.device (SANA-II). Dual
# library + MUI GUI. See add_poseidon_class().
add_poseidon_class(moschipeth BASETYPE NepEthBase VERSION 4 REVISION 3 PRI 48 GUI
    SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers)
