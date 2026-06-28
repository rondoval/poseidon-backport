# camdusbmidi.class — USB-MIDI -> CAMD. Single library + MUI config GUI (libbase MUI base,
# simplemidi-style). Registers a tiny CAMD driver in DEVS:Midi/ at bind time from the prebuilt
# m68k blob CAMDDriver.c (the #if __mc68000__ path) — so no CAMD SDK is needed to build.
# The camd/ subdir holds the driver *source* (poseidonusb.c) the blob was generated from; not built.
add_poseidon_class(camdusbmidi BASETYPE NepHidBase VERSION 4 REVISION 5 PRI 39 GUI DEFINES HAS_LIBOPEN)
