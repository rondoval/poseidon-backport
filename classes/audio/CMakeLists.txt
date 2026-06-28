# usbaudio.class — USB audio -> ahi.device sub-driver. Dual library: the class (NepAudioBase)
# embeds an AHI sub-driver library "usbaudio.audio" (NepAudioSubLibBase, MakeLibrary'd in libInit
# from SubLibFuncTable[] — the 19 subLib* AHI vectors). MUI config GUI (libbase MUI base). The AHI
# library/inline headers ship in the bebbo toolchain; ahi.device is opened per-binding at runtime.
add_poseidon_class(usbaudio BASETYPE NepAudioBase VERSION 4 REVISION 3 PRI -120 GUI
    SOURCES numtostr.c DEFINES HAS_LIBOPEN)
