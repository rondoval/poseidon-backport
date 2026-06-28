# printer.class — USB printer (Centronics/IEEE-1284 class) -> usbparallel.device. Dual library
# (class NepPrinterBase + embedded usbparallel.device via dev.c) with a MUI config GUI.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(printer BASETYPE NepPrinterBase VERSION 4 REVISION 3 PRI 48 GUI
    SOURCES dev.c DEFINES HAS_LIBOPEN)
