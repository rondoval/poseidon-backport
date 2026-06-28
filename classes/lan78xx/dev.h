
/* DEVICE STUFF */

#ifndef LAN78XX_DEV_H
#define LAN78XX_DEV_H

#define DEVNAME "usblan78xx.device"

#define DEVBASETYPEPTR struct NepEthDevBase *

/* forward declarations */
struct NepClassEth;
struct BufMan;

/* local protos */

DEVBASETYPEPTR devInit(DEVBASETYPEPTR base asm("d0"), BPTR seglist asm("a0"), struct ExecBase * SysBase asm("a6"));

/* The usblan78xx.device LVO vectors (order 1..6 = DevFuncTable order). Defined in dev.c. */
DEVBASETYPEPTR devOpen(struct IOSana2Req * ioreq asm("a1"), ULONG unitnum asm("d0"), ULONG flags asm("d1"), DEVBASETYPEPTR base asm("a6"));
BPTR           devClose(struct IOSana2Req * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));
BPTR           devExpunge(DEVBASETYPEPTR extralh asm("d0"), DEVBASETYPEPTR base asm("a6"));
DEVBASETYPEPTR devReserved(DEVBASETYPEPTR base asm("a6"));
void           devBeginIO(struct IOSana2Req * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));
LONG           devAbortIO(struct IOSana2Req * ioreq asm("a1"), DEVBASETYPEPTR base asm("a6"));

/* Device stuff */

#define deverror(ioerr, wireerr) (((wireerr) << 8) | ((ioerr)&0xff))

/* Reply the iorequest with success
 */
#define RC_OK 0

/* Magic cookie, don't set error fields & don't reply the ioreq
 */
#define RC_DONTREPLY -1

struct Unit *Open_Unit(struct IOSana2Req *ioreq, LONG unitnr, struct NepEthDevBase *base);
void Close_Unit(struct NepEthDevBase *base, struct NepClassEth *ncp, struct IOSana2Req *ioreq);

WORD cmdNSDeviceQuery(struct NepClassEth *ncp, struct IOStdReq *ioreq);

LONG AbortReq(struct NepClassEth *ncp, struct List *list, struct IOSana2Req *ioreq);
void TermIO(struct NepClassEth *ncp, struct IOSana2Req *ioreq);
void AbortList(struct NepClassEth *ncp, struct List *list, struct BufMan *bufman, WORD error);
void AbortRW(struct NepClassEth *ncp, struct BufMan *bufman, WORD error);
struct Sana2PacketTypeStats *FindPacketTypeStats(struct NepClassEth *ncp, ULONG packettype);
WORD AddMCastRange(struct NepClassEth *ncp, struct IOSana2Req *ioreq, UBYTE *lower, UBYTE *upper);
WORD DelMCastRange(struct NepClassEth *ncp, struct IOSana2Req *ioreq, UBYTE *lower, UBYTE *upper);
void UpdateMulticastHash(struct NepClassEth *ncp);

struct my_NSDeviceQueryResult {
    ULONG DevQueryFormat;           /* this is type 0               */
    ULONG SizeAvailable;            /* bytes available              */
    UWORD DeviceType;               /* what the device does         */
    UWORD DeviceSubType;            /* depends on the main type     */
    const UWORD *SupportedCommands; /* 0 terminated list of cmd's   */
};

#endif /* LAN78XX_DEV_H */
