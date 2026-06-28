
/* DEVICE STUFF */

#define DEVNAME             "usbstir4200.device"

#define DEVBASETYPEPTR struct NepSTIrDevBase *

/* local protos */

DEVBASETYPEPTR devInit(DEVBASETYPEPTR base asm("d0"), BPTR seglist asm("a0"), struct ExecBase * SysBase asm("a6"));

DEVBASETYPEPTR devOpen(struct IOIrDAReq * ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"), DEVBASETYPEPTR base asm("a6"));

BPTR devClose(struct IOIrDAReq * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

BPTR devExpunge(DEVBASETYPEPTR extralh asm("d0"), DEVBASETYPEPTR base asm("a6"));

DEVBASETYPEPTR devReserved(DEVBASETYPEPTR base asm("a6"));

void devBeginIO(struct IOIrDAReq * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

LONG devAbortIO(struct IOIrDAReq * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

WORD cmdQueryDevice(struct IOIrDAReq *ioreq,
                    struct NepClassSTIr4200 *ncp,
                    struct NepSTIrDevBase *base);

/* Device stuff */

/* Reply the iorequest with success
*/
#define RC_OK         0

/* Magic cookie, don't set error fields & don't reply the ioreq
*/
#define RC_DONTREPLY  -1

struct Unit *Open_Unit(struct IOIrDAReq *ioreq,
                       LONG unitnr,
                       struct NepSTIrDevBase *base);
void Close_Unit(struct NepSTIrDevBase *base, struct NepClassSTIr4200 *ncp,
                struct IOIrDAReq *ioreq);

WORD cmdNSDeviceQuery(struct IOStdReq *ioreq, struct NepClassSTIr4200 *ncp, struct NepSTIrDevBase *base);

void TermIO(struct IOIrDAReq *ioreq, struct NepSTIrDevBase *base);


struct my_NSDeviceQueryResult
{
    ULONG   DevQueryFormat;         /* this is type 0               */
    ULONG   SizeAvailable;          /* bytes available              */
    UWORD   DeviceType;             /* what the device does         */
    UWORD   DeviceSubType;          /* depends on the main type     */
    const UWORD *SupportedCommands; /* 0 terminated list of cmd's   */
};
