#ifndef MIDI_CAMDDEVICES_H
#define MIDI_CAMDDEVICES_H
/*
 * midi/camddevices.h — native AmigaOS (m68k) CAMD MIDI device-driver interface.
 *
 * A CAMD MIDI driver is a LoadSeg'able file in DEVS:Midi/. Its first hunk begins with a
 * 4-byte `moveq #-1,d0; rts` stub (so it can't be run as a program) immediately followed
 * by a `struct MidiDeviceData` whose Magic == MDD_Magic. camd.library loads the file,
 * reads the table at that fixed offset, and calls the entry points using the register
 * convention noted on each field.
 *
 * Self-contained variant of the AROS midi/camddevices.h (Kjetil Matheussen / AROS team)
 * for the bebbo NDK toolchain — no <libcore/compiler.h> (the driver's own functions carry
 * the asm("aN") register specs; the table entries are assigned via (APTR) casts).
 */

#include <exec/types.h>

struct MidiPortData {
    void (*ActivateXmit)(APTR userdata, ULONG portnum);   /* userdata A2, portnum D0 */
};

struct MidiDeviceData {
    ULONG  Magic;                 /* == MDD_Magic ('MDEV') */
    char  *Name;
    char  *IDString;
    UWORD  Version;
    UWORD  Revision;
    BOOL  (*Init)(APTR SysBase);  /* called right after LoadSeg(); SysBase in A6 */
    void  (*Expunge)(void);       /* called right before UnLoadSeg() */
    struct MidiPortData *(*OpenPort)(struct MidiDeviceData *data, LONG portnum,
                                     ULONG (*transmitfunc)(APTR userdata),
                                     void (*receivefunc)(UWORD input, APTR userdata),
                                     APTR userdata);
                                  /* data A3, portnum D0, transmit A0, receive A1, userdata A2 */
    void  (*ClosePort)(struct MidiDeviceData *data, LONG portnum);  /* data A3, portnum D0 */
    UBYTE  NPorts;                /* number of ports (may be set in Init) */
    UBYTE  Flags;                 /* 0 = old format, 1 = new */
};

#define MDD_Magic ((ULONG)'M' << 24 | (ULONG)'D' << 16 | 'E' << 8 | 'V')

#endif /* MIDI_CAMDDEVICES_H */
