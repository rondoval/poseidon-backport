
#include <proto/exec.h>
#include <proto/camdusbmidi.h>

#include <exec/types.h>
#include <midi/camddevices.h>
#include "camdusbmidi.h"

typedef ULONG (*camdTransmitFunc)(APTR driverdata);
typedef void (*camdReceiveFunc)(UWORD input, APTR driverdata);

/* CAMD driver entry points. camd.library LoadSeg()s this file and calls them
 * through the MidiDeviceData table below; the register convention is fixed by
 * <midi/camddevices.h>: Init gets SysBase in A6; OpenPort takes data A3,
 * portnum D0, transmitfunc A0, receivefunc A1, userdata A2; ClosePort takes
 * data A3, portnum D0. */
extern BOOL Init(APTR sysbase asm("a6"));
extern VOID Expunge(VOID);
extern struct MidiPortData *OpenPort(struct MidiDeviceData *data asm("a3"), LONG portnum asm("d0"),
                                     APTR transmitfunc asm("a0"), APTR receivefunc asm("a1"),
                                     APTR userdata asm("a2"));
extern VOID ClosePort(struct MidiDeviceData *data asm("a3"), LONG portnum asm("d0"));

VOID ActivateXmit(APTR userdata, LONG portnum);

char name[], vers[];

/*** Identification data must follow directly *********************************/

#define CAMDPORTCOUNT   16

static struct MidiDeviceData MidiDeviceData =
{
  MDD_Magic,
  name,
  vers,
  1, 2,
  (APTR)Init,
  (APTR)Expunge,
  (APTR)OpenPort,
  (APTR)ClosePort,
  CAMDPORTCOUNT,
  0
};

char name[] = "poseidonusb";
char vers[] = "$VER: Poseidon USB camdusbmidi.class driver 1.2 (14.02.2020)";

struct ExecBase *SysBase = NULL;
struct Library *nh = NULL;

BOOL Init(APTR sysbase asm("a6"))
{
    SysBase = sysbase;          /* CAMD passes the exec base in A6 (camddevices.h) */
    nh = OpenLibrary("camdusbmidi.class", 0);

    return (nh != NULL);
}


VOID Expunge(VOID)
{
    if (nh)
        CloseLibrary(nh);
    nh = NULL;
}

/* RX hook (struct Hook h_Entry: hook A0, object A2, message A1). */
static void SendToCAMD(struct Hook *hook asm("a0"), void *data asm("a2"), ULONG *params asm("a1"))
{
    struct CAMDAdapter *port = (struct CAMDAdapter *)hook->h_Data;
    ULONG len = *(ULONG *)data;
    UBYTE *msg = (UBYTE *)((IPTR)data + sizeof(ULONG));

    camdReceiveFunc portReceive = (camdReceiveFunc)port->ca_RXFunc;

    while (len > 0)
    {
        portReceive(*msg++, port->ca_UserData);

        len--;
    }
}

/* TX soft-interrupt (struct Interrupt is_Code; Cause()'d, is_Data = port in A1). */
static void GetFromCAMD(struct Hook *hook asm("a0"), Object *obj asm("a2"), struct CAMDAdapter *port asm("a1"))
{
    ULONG val, pos, sent = 0;

    camdTransmitFunc portTransmit = (camdTransmitFunc)port->ca_TXFunc;

    do
    {
        pos = port->ca_TXWritePos;
        val = portTransmit(port->ca_UserData);
        port->ca_TXBuffer[pos] = val;
        pos++;
        pos &= port->ca_TXBufSize;
        if (pos == port->ca_TXReadPos)
            break;
        port->ca_TXWritePos = pos;
        sent++;
    } while (sent < 100); // TODO: Not sure what to do here??

    Signal(port->ca_MsgPort->mp_SigTask, (1 << port->ca_MsgPort->mp_SigBit));
}

struct CAMDAdapter *CAMDPortBases[CAMDPORTCOUNT] = { NULL };

struct MidiPortData *OpenPort(struct MidiDeviceData *data asm("a3"), LONG portnum asm("d0"),
                              APTR transmitfunc asm("a0"), APTR receivefunc asm("a1"),
                              APTR userdata asm("a2"))
{
    CAMDPortBases[portnum] = usbCAMDOpenPort(transmitfunc, receivefunc, userdata, name, portnum);
    if (CAMDPortBases[portnum])
    {
        CAMDPortBases[portnum]->ca_ActivateFunc = ActivateXmit;
        CAMDPortBases[portnum]->ca_CAMDRXFunc.h_Entry = (APTR)SendToCAMD;
        CAMDPortBases[portnum]->ca_CAMDRXFunc.h_Data = CAMDPortBases[portnum];
        CAMDPortBases[portnum]->ca_CAMDTXFunc.is_Code = (APTR)GetFromCAMD;
        CAMDPortBases[portnum]->ca_CAMDTXFunc.is_Data = CAMDPortBases[portnum];
        CAMDPortBases[portnum]->ca_IsOpen = TRUE;
    }
    return (struct MidiPortData *)&CAMDPortBases[portnum]->ca_ActivateFunc;
}

VOID ClosePort(struct MidiDeviceData *data asm("a3"), LONG portnum asm("d0"))
{
    if (CAMDPortBases[portnum])
    {
        CAMDPortBases[portnum]->ca_IsOpen = FALSE;
        usbCAMDClosePort(portnum, name);
    }
    CAMDPortBases[portnum] = NULL;
}

VOID ActivateXmit(APTR userdata, LONG portnum)
{
    int i;
    for (i = 0; i < CAMDPORTCOUNT; i++)
    {
        if (CAMDPortBases[i])
            Cause(&CAMDPortBases[i]->ca_CAMDTXFunc);
    }
}
