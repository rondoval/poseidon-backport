
/* DEVICE STUFF */

#define DEVNAME             "usbraw.device"

#define DEVBASETYPEPTR struct NepRawDevBase *

/* local protos */

DEVBASETYPEPTR devInit(DEVBASETYPEPTR base asm("d0"), BPTR seglist asm("a0"), struct ExecBase * SysBase asm("a6"));

DEVBASETYPEPTR devOpen(struct IOStdReq * ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"), DEVBASETYPEPTR base asm("a6"));

BPTR devClose(struct IOStdReq * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

BPTR devExpunge(DEVBASETYPEPTR extralh asm("d0"), DEVBASETYPEPTR base asm("a6"));

DEVBASETYPEPTR devReserved(DEVBASETYPEPTR base asm("a6"));

void devBeginIO(struct IOStdReq * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

LONG devAbortIO(struct IOStdReq * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

/* Device stuff */

/* Reply the iorequest with success
*/
#define RC_OK         0

/* Magic cookie, don't set error fields & don't reply the ioreq
*/
#define RC_DONTREPLY  -1

struct Unit *Open_Unit(struct IOStdReq *ioreq,
                       LONG unitnr,
                       struct NepRawDevBase *base);
void Close_Unit(struct NepRawDevBase *base, struct NepClassRawWrap *ncp,
                struct IOStdReq *ioreq);

WORD cmdNSDeviceQuery(struct IOStdReq *ioreq, struct NepClassRawWrap *ncp, struct NepRawDevBase *base);

void TermIO(struct IOStdReq *ioreq, struct NepRawDevBase *base);

struct my_NSDeviceQueryResult
{
    ULONG   DevQueryFormat;         /* this is type 0               */
    ULONG   SizeAvailable;          /* bytes available              */
    UWORD   DeviceType;             /* what the device does         */
    UWORD   DeviceSubType;          /* depends on the main type     */
    const UWORD *SupportedCommands; /* 0 terminated list of cmd's   */
};
