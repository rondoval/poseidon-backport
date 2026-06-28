/* camd_vectors.h — camdusbmidi.class's extra library vectors, injected into the
 * shared class skeleton's funcTable via -DCLASS_VECTORS_HDR (see classes/class_main.c).
 *
 * The original genmodule .conf declared, after the 3 usbclass vectors:
 *     .skip 7
 *     APTR usbCAMDOpenPort(xmitfc,recvfct,userdata,idstr,port) (A0,A1,A2,A3,D0)   // LVO -90
 *     VOID usbCAMDClosePort(port,idstr)                        (D0,A1)            // LVO -96
 * so the embedded CAMD MIDI driver (camd/poseidonusb.c) can OpenLibrary the class and
 * call these two vectors. The 7 reserved slots map to LibNull (a harmless no-op).
 */
extern LONG usbCAMDOpenPort(void);    /* address-only, like usbGetAttrsA in class_main.c */
extern LONG usbCAMDClosePort(void);

#define CLASS_EXTRA_VECTORS \
    (APTR)LibNull,(APTR)LibNull,(APTR)LibNull,(APTR)LibNull,(APTR)LibNull,(APTR)LibNull,(APTR)LibNull, \
    (APTR)usbCAMDOpenPort,(APTR)usbCAMDClosePort,
