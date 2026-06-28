
/* DEVICE STUFF */

#define DEVNAME             "serialcp210x.device"

#define DEVBASETYPEPTR struct NepSerDevBase *

/* local protos */

DEVBASETYPEPTR devInit(DEVBASETYPEPTR base asm("d0"), BPTR seglist asm("a0"), struct ExecBase * SysBase asm("a6"));

/* The usbmodem.device LVO vectors (order 1..6 = DevFuncTable order). Defined in dev.c
   with bebbo register args; the embedded-device skeleton builds the funcTable. */
DEVBASETYPEPTR devOpen(struct IOExtSer * ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"), DEVBASETYPEPTR base asm("a6"));
BPTR           devClose(struct IOExtSer * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));
BPTR           devExpunge(DEVBASETYPEPTR extralh asm("d0"), DEVBASETYPEPTR base asm("a6"));
DEVBASETYPEPTR devReserved(DEVBASETYPEPTR base asm("a6"));
void           devBeginIO(struct IOExtSer * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));
LONG           devAbortIO(struct IOExtSer * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

/* Device stuff */

/* Reply the iorequest with success
*/
#define RC_OK         0

/* Magic cookie, don't set error fields & don't reply the ioreq
*/
#define RC_DONTREPLY  -1

struct Unit *Open_Unit(struct IOExtSer *ioreq,
                       LONG unitnr,
                       struct NepSerDevBase *base);
void Close_Unit(struct NepSerDevBase *base, struct NepClassSerial *ncp,
                struct IOExtSer *ioreq);

WORD cmdNSDeviceQuery(struct IOStdReq *ioreq, struct NepClassSerial *ncp, struct NepSerDevBase *base);

void TermIO(struct IOExtSer *ioreq, struct NepSerDevBase *base);

struct my_NSDeviceQueryResult
{
    ULONG   DevQueryFormat;         /* this is type 0               */
    ULONG   SizeAvailable;          /* bytes available              */
    UWORD   DeviceType;             /* what the device does         */
    UWORD   DeviceSubType;          /* depends on the main type     */
    const UWORD *SupportedCommands; /* 0 terminated list of cmd's   */
};
