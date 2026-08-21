# ethwrap.class — generic SANA-II Ethernet wrapper -> usbethwrap.device. Dual library + MUI GUI.
# See add_poseidon_class().
add_poseidon_class(ethwrap BASETYPE NepEthBase GUI
    SOURCES dev.c DEFINES HAS_LIBOPEN DEPS sana2_headers)
