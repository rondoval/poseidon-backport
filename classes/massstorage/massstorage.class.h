#ifndef MASSSTORAGE_CLASS_H
#define MASSSTORAGE_CLASS_H

/*
 *----------------------------------------------------------------------------
 *                         Includes for MS class
 *----------------------------------------------------------------------------
 *                   By Chris Hodges <chrisly@platon42.de>
 */

#include "common.h"

#include <libraries/expansion.h>
#include <libraries/expansionbase.h>
#include <libraries/configregs.h>
#include <libraries/configvars.h>
#include <libraries/asl.h>

#include <dos/dostags.h>
#include <scsi/commands.h>
#include <scsi/values.h>
#include <dos/dosextens.h>
#include <dos/filehandler.h>
#include <resources/filesysres.h>

#include <devices/usb.h>
#include <devices/usbhardware.h>
#include <devices/usb_massstorage.h>
#include <libraries/usbclass.h>

#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include "massstorage.h"
#include "dev.h"

#ifndef TD_READ64
#define TD_READ64     24
#define TD_WRITE64    25
#define TD_SEEK64     26
#define TD_FORMAT64   27
#endif

/* Protos */

struct NepClassMS * usbAttemptInterfaceBinding(struct NepMSBase *nh, struct PsdInterface *pif);
struct NepClassMS * usbForceInterfaceBinding(struct NepMSBase *nh, struct PsdInterface *pif);
void usbReleaseInterfaceBinding(struct NepMSBase *nh, struct NepClassMS *ncm);

struct NepClassMS * nAllocMS(void);
void nFreeMS(struct NepClassMS *ncm);

BOOL nLoadClassConfig(struct NepMSBase *nh);
BOOL nLoadBindingConfig(struct NepClassMS *ncm);
LONG nOpenBindingCfgWindow(struct NepMSBase *nh, struct NepClassMS *ncm);

void nGUITaskCleanup(struct NepClassMS *ncm);
BOOL nStoreConfig(struct NepClassMS *ncm);

void nHexString(UBYTE *src, ULONG len, UBYTE *buf);

LONG nScsiDirect(struct NepClassMS *ncm, struct SCSICmd *scsicmd);
LONG nScsiDirectBulk(struct NepClassMS *ncm, struct SCSICmd *scsicmd);
LONG nScsiDirectCBI(struct NepClassMS *ncm, struct SCSICmd *scsicmd);
LONG nScsiDirectUAS(struct NepClassMS *ncm, struct SCSICmd *scsicmd);

/* UAS multi-tag engine (massstorage_uas.c) */
BOOL nUasCollectEndpoints(struct NepClassMS *ncm);
BOOL nUasInitTags(struct NepClassMS *ncm);
void nUasDisableTags(struct NepClassMS *ncm);
BOOL nUasEligible(struct NepClassMS *ncm, struct IOStdReq *ioreq);
struct UasTag * nUasFreeTag(struct NepClassMS *ncm);
void nUasSubmitTag(struct NepClassMS *ncm, struct UasTag *ut, struct IOStdReq *ioreq);
void nUasProcessAborts(struct NepClassMS *ncm);
void nUasReapTags(struct NepClassMS *ncm);
void nUasDrainTags(struct NepClassMS *ncm);
BOOL nUasTagsIdle(struct NepClassMS *ncm); /* TRUE = no tag in flight (non-UAS: always) */
LONG nBulkReset(struct NepClassMS *ncm);
LONG nBulkClear(struct NepClassMS *ncm);
void nLockXFer(struct NepClassMS *ncm);
void nUnlockXFer(struct NepClassMS *ncm);
void nSetNakTimeout(struct NepClassMS *ncm, struct PsdPipe *pp, ULONG timeout_ms);
void nApplyNakTimeout(struct NepClassMS *ncm, ULONG timeout_ms);
UWORD nBuildRWCdb(UBYTE *cdb, BOOL iswrite, ULONG startblockhigh, ULONG startblock,
                  ULONG datalen, UWORD blockshift);
LONG nRead64(struct NepClassMS *ncm, struct IOStdReq *ioreq);
LONG nWrite64(struct NepClassMS *ncm, struct IOStdReq *ioreq);
LONG nFormat64(struct NepClassMS *ncm, struct IOStdReq *ioreq);
LONG nSeek64(struct NepClassMS *ncm, struct IOStdReq *ioreq);
LONG nGetGeometry(struct NepClassMS *ncm, struct IOStdReq *ioreq);
LONG nGetWriteProtect(struct NepClassMS *ncm);
LONG nStartStop(struct NepClassMS *ncm, struct IOStdReq *ioreq);

BOOL nStartRemovableTask(struct Library *ps, struct NepMSBase *nh);
struct NepMSBase * nAllocRT(void);
void nFreeRT(struct NepMSBase *nh);
void nUnmountPartition(struct NepClassMS *ncm);
LONG nIOCmdTunnel(struct NepClassMS *ncm, struct IOStdReq *ioreq);
LONG nScsiDirectTunnel(struct NepClassMS *ncm, struct SCSICmd *scsicmd);

struct DeviceNode * FindMatchingDevice(struct NepClassMS *ncm);

void AutoDetectMaxTransfer(struct NepClassMS *ncm);

void nMSTask();
void nRemovableTask();
void nGUITask();

LONG LUNListDisplayHook(struct Hook * hook asm("a0"), char ** strarr asm("a2"), struct NepClassMS * ncm asm("a1"));

#endif /* MASSSTORAGE_CLASS_H */
