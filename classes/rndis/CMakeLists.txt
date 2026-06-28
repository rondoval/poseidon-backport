# rndis.class — USB RNDIS (Windows/phone tethering) -> usbrndis.device (SANA-II). Dual library, no
# GUI; +if_urndis.c (the BSD-derived RNDIS core). See add_poseidon_class().
add_poseidon_class(rndis BASETYPE NepEthBase VERSION 1 REVISION 2 PRI 48
    SOURCES dev.c if_urndis.c DEPS sana2_headers mui_headers)
