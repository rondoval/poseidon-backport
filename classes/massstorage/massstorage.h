#ifndef MASSSTORAGE_H
#define MASSSTORAGE_H

#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <libraries/mui.h>
#include <libraries/gadtools.h>
#include <devices/newstyle.h>
#include <devices/trackdisk.h>
#include <devices/scsidisk.h>
#include <devices/usbhardware.h>
#include <devices/usb_massstorage.h> /* struct UasCommandIU (struct UasTag below) */

#if defined(__GNUC__)
# pragma pack(2)
#endif

#define ID_ABOUT        0x55555555
#define ID_STORE_CONFIG 0xaaaaaaaa
#define ID_DEF_CONFIG   0xaaaaaaab
#define ID_SELECT_LUN   0x22222222
#define ID_AUTODTXMAXTX 0x11111111

/* TRUE for "device sent more data than requested". UHCI/OHCI/EHCI HCDs report
   this as UHIOERR_OVERFLOW; xhci folds the same wire condition into Babble
   Detected (UHIOERR_BABBLE). The transports treat both as benign truncation. */
static inline BOOL nIsOverflowErr(LONG ioerr)
{
    return (ioerr == UHIOERR_OVERFLOW) || (ioerr == UHIOERR_BABBLE);
}

struct ClsDevCfg
{
    ULONG cdc_ChunkID;
    ULONG cdc_Length;
    IPTR  cdc_NakTimeout;
    IPTR  cdc_PatchFlags;
    char  cdc_FATFSName[64];
    ULONG cdc_FATDosType;
    IPTR  cdc_StartupDelay;
    char  cdc_FATControl[64];
    ULONG cdc_MaxTransfer;
    char  cdc_CDFSName[64];
    ULONG cdc_CDDosType;
    char  cdc_CDControl[64];
    char  cdc_NTFSName[64];
    ULONG cdc_NTFSDosType;
    char  cdc_NTFSControl[64];
    /* appended fields only: the chunk loader min()s on cdc_Length, so an old
       stored config leaves new trailing fields at their defaults */
    IPTR  cdc_UasQueueDepth;  /* UAS tag-engine queue depth (1..NCM_MAXTAGS) */
};

struct ClsUnitCfg
{
    ULONG cuc_ChunkID;
    ULONG cuc_Length;
    IPTR  cuc_AutoMountLegacy;
    char  cuc_DOSName[32];
    IPTR  cuc_Buffers;
    IPTR  cuc_AutoMountRDB;
    IPTR  cuc_Boot;
    IPTR  cuc_DefaultUnit;
    IPTR  cuc_AutoUnmount;
    IPTR  cuc_MountAllLegacy;
    IPTR  cuc_AutoMountCD;
};

#if defined(__GNUC__)
# pragma pack()
#endif

#define PFF_SINGLE_LUN     0x000001 /* allow access only to LUN 0 */
#define PFF_MODE_XLATE     0x000002 /* translate 6 byte commands to 10 byte commands */
#define PFF_EMUL_LARGE_BLK 0x000004 /* Emulate access on larger block sizes */
#define PFF_REM_SUPPORT    0x000010 /* extended removable support */
#define PFF_FIX_INQ36      0x000040 /* inquiry request needs fixing */
#define PFF_DELAY_DATA     0x000080 /* delay data phase */
#define PFF_SIMPLE_SCSI    0x000100 /* kill all complicated SCSI commands that might crash firmware */
#define PFF_NO_RESET       0x000200 /* do not perform initial bulk reset */
#define PFF_FAKE_INQUIRY   0x000400 /* filter inquiry and fake it */
#define PFF_FIX_CAPACITY   0x000800 /* fix capacity by one */
#define PFF_NO_FALLBACK    0x001000 /* don't fall back automatically */
#define PFF_CSS_BROKEN     0x002000 /* olympus command status signature fix */
#define PFF_CLEAR_EP       0x004000 /* clear endpoint halt */
#define PFF_DEBUG          0x008000 /* more debug output */
#define PFF_NO_UAS         0x010000 /* do not prefer the UAS alternate (force Bulk-Only) */

/* UAS multi-tag engine: one outstanding SCSI command per tag; the tag doubles
   as the USB3 stream id its status/data pipes ride (UAS 1.0: the device
   mirrors the Command IU tag as the stream selector). */
#define NCM_MAXTAGS 16

#define UTS_FREE       0 /* no client request bound */
#define UTS_RUNNING    1 /* chunk on the wire (pipes armed) */
#define UTS_QUARANTINE 2 /* host-side kill: the DEVICE still owns the command,
                            so the tag must not be reused until an ABORT TASK
                            TMF is answered (or the reset escalation runs) —
                            otherwise the old command's Sense IU lands in the
                            reused tag's fresh status transfer */

struct UasTag
{
    struct IOStdReq    *ut_IOReq;         /* client request being served */
    struct PsdPipe     *ut_StatusPipe;    /* per-tag pipes, PPA_StreamID == ut_Tag */
    struct PsdPipe     *ut_DataInPipe;
    struct PsdPipe     *ut_DataOutPipe;
    UWORD               ut_Tag;           /* UAS tag == stream id (1-based) */
    UWORD               ut_State;         /* UTS_* */
    UWORD               ut_Outstanding;   /* pipe replies still owed (0..2) */
    BOOL                ut_StatusArmed;   /* status pipe reply outstanding */
    BOOL                ut_DataArmed;     /* data pipe reply outstanding */
    BOOL                ut_AbortReq;      /* devAbortIO wants this request dead */
    BOOL                ut_Failed;        /* chunk failed; finalize once fully reaped */
    LONG                ut_IOErr;         /* first pipe error of the failed chunk */
    BOOL                ut_IsRead;
    /* device-side ownership of the current chunk: the Command IU reached the
       device (ut_CmdSent) and no Sense IU has closed it (ut_StatusSeen).
       Both true at finalize time = the device still owns the tag = quarantine. */
    BOOL                ut_CmdSent;
    BOOL                ut_StatusSeen;
    /* chunk progress: large transfers chunk on the same tag */
    UBYTE              *ut_Data;          /* client buffer */
    ULONG               ut_Offset;        /* bytes completed */
    ULONG               ut_Remain;        /* bytes still to transfer */
    ULONG               ut_ChunkLen;      /* current chunk byte count */
    ULONG               ut_StartBlock;    /* next chunk's LBA (low) */
    ULONG               ut_StartBlockHigh;/* next chunk's LBA (high, >2TB) */
    struct UasCommandIU ut_CmdIU;         /* command IU wire buffer */
    UBYTE               ut_StatusBuf[64]; /* Sense IU landing buffer */
};

struct NepClassMS
{
    struct Unit         ncm_Unit;         /* Unit structure */
    struct NepClassMS  *ncm_UnitLUN0;     /* Head of list */
    ULONG               ncm_UnitNo;       /* Unit number */
    struct NepMSBase   *ncm_ClsBase;      /* Up linkage */
    struct NepMSDevBase *ncm_DevBase;     /* Device base */
    struct Library     *ncm_Base;         /* Poseidon base */
    struct PsdDevice   *ncm_Device;       /* Up linkage */
    struct PsdConfig   *ncm_Config;       /* Up linkage */
    struct PsdInterface *ncm_Interface;   /* Up linkage */
    struct Task        *ncm_ReadySigTask; /* Task to send ready signal to */
    LONG                ncm_ReadySignal;  /* Signal to send when ready */
    struct Task        *ncm_Task;         /* Subtask */
    struct MsgPort     *ncm_TaskMsgPort;  /* Message Port of Subtask */
    struct SignalSemaphore ncm_XFerLock;  /* LUN allowed to talk to the device */
    struct PsdPipe     *ncm_EP0Pipe;      /* Endpoint 0 pipe */
    struct PsdEndpoint *ncm_EPOut;        /* Endpoint OUT */
    struct PsdPipe     *ncm_EPOutPipe;    /* Endpoint OUT pipe */
    struct PsdEndpoint *ncm_EPIn;         /* Endpoint IN */
    struct PsdPipe     *ncm_EPInPipe;     /* Endpoint IN pipe */
    struct PsdEndpoint *ncm_EPCmd;        /* UAS Command Endpoint */
    struct PsdPipe     *ncm_EPCmdPipe;    /* UAS Command pipe */
    struct PsdEndpoint *ncm_EPStatus;     /* UAS Status Endpoint */
    struct PsdEndpoint *ncm_EPInt;        /* Optional Endpoint INT */
    struct PsdPipe     *ncm_EPIntPipe;    /* Optional Endpoint INT pipe */
    UWORD               ncm_EPOutNum;     /* Endpoint OUT number */
    UWORD               ncm_EPInNum;      /* Endpoint IN number */
    UWORD               ncm_EPCmdNum;     /* UAS Command endpoint number */
    UWORD               ncm_EPStatusNum;  /* UAS Status endpoint number */
    UWORD               ncm_EPIntNum;     /* Endpoint INT number */
    struct MsgPort     *ncm_DevMsgPort;   /* Message Port for IOParReq */
    UWORD               ncm_UnitProdID;   /* ProductID of unit */
    UWORD               ncm_UnitVendorID; /* VendorID of unit */
    UWORD               ncm_UnitIfNum;    /* Interface number */
    UWORD               ncm_UnitLUN;      /* LUN */
    UWORD               ncm_MaxLUN;       /* Number of LUNs */
    BOOL                ncm_DenyRequests; /* Device is gone! */
    BOOL                ncm_HasMounted;   /* Has mounted some partitions */
    BOOL                ncm_UnitReady;    /* Unit is ready */
    ULONG               ncm_ChangeCount;  /* Number of disk changes */
    ULONG               ncm_LastChange;   /* Change number last interrupt created */
    struct List         ncm_DCInts;       /* DiskChange Interrupt IORequests */
    ULONG               ncm_BlockSize;    /* BlockSize = 512 */
    ULONG               ncm_BlockShift;   /* Log2 BlockSize */
    BOOL                ncm_WriteProtect; /* Is Disk write protected? */
    BOOL                ncm_Removable;    /* Is disk removable? */
    BOOL                ncm_ForceRTCheck; /* Force removable task to be restarted */
    BOOL                ncm_Ejected;      /* safe-eject latch: suppress re-mount until replug */
    UWORD               ncm_DeviceType;   /* Peripheral Device Type (from Inquiry data) */
    UWORD               ncm_TPType;       /* Transport type */
    UWORD               ncm_CSType;       /* SCSI Commandset type */
    ULONG               ncm_DmaAlign;     /* Cached HCD DMA buffer alignment (bytes), 0 = default */
    ULONG               ncm_TagCount;     /* Tag for CBW (BOT) */
    UWORD               ncm_UasQueueDepth;/* latched tag-engine queue depth (>= 1 while UAS-bound) */
    struct UasTag       ncm_UasTags[NCM_MAXTAGS]; /* tag contexts (first ncm_UasQueueDepth are live) */
    /* Task Management transport (ABORT TASK for a quarantined tag). The TM IU
       carries its own tag, so it needs a status stream of its own: stream id
       QD+1 when the device has the headroom (ncm_UasTMTag), else 0 = borrow
       mode, which drains the other tags and rides a free tag's status pipe. */
    struct PsdPipe     *ncm_UasTMStatusPipe;
    UWORD               ncm_UasTMTag;     /* reserved TM stream id, 0 = borrow mode */
    UWORD               ncm_UasTMOwnTag;  /* tag the TM IU in flight identifies as */
    UWORD               ncm_UasTMTargetTag; /* tag being aborted, 0 = no TMF in flight */
    struct PsdPipe     *ncm_UasTMArmed;   /* status pipe of the TMF in flight, NULL = none */
    UBYTE               ncm_UasTMBuf[64]; /* Response IU landing buffer */
    struct DriveGeometry ncm_Geometry;    /* Drive Geometry */
    ULONG               ncm_GeoChangeCount; /* when did we last obtained the geometry for caching */
    BOOL                ncm_BulkResetBorks; /* Bulk Reset is broken, don't try to use it */

    UBYTE              *ncm_OneBlock;     /* buffer for one block */
    ULONG               ncm_OneBlockSize; /* size of one block buffer */

    STRPTR              ncm_DevIDString;  /* Device ID String */
    STRPTR              ncm_IfIDString;   /* Interface ID String */

    struct List         ncm_XFerQueue;    /* List of xfer requests */

    char                ncm_LUNIDStr[18];
    char                ncm_LUNNumStr[4];
    UBYTE               ncm_ModePageBuf[256];

    BOOL                ncm_UsingDefaultCfg;

    BOOL                ncm_IOStarted;    /* IO Running */
    BOOL                ncm_Running;      /* Not suspended */
    UWORD               ncm_CmdBusy;      /* nScsiDirect nesting depth (unit task
                                             writes; suspend probe reads cross-task) */

    struct ClsDevCfg   *ncm_CDC;
    struct ClsUnitCfg  *ncm_CUC;

    struct Library     *ncm_MUIBase;      /* MUI master base */
    struct Library     *ncm_PsdBase;      /* Poseidon base */
    struct Library     *ncm_IntBase;      /* Intuition base */
    struct Task        *ncm_GUITask;      /* GUI Task */
    struct NepClassMS  *ncm_GUIBinding;   /* Window of binding that's open */

    Object             *ncm_App;
    Object             *ncm_MainWindow;
    Object             *ncm_NakTimeoutObj;
    Object             *ncm_UasQDObj;
    Object             *ncm_SingleLunObj;
    Object             *ncm_FixInquiryObj;
    Object             *ncm_FakeInquiryObj;
    Object             *ncm_SimpleSCSIObj;
    Object             *ncm_XLate610Obj;
    Object             *ncm_CSSBrokenObj;
    Object             *ncm_EmulLargeBlkObj;
    Object             *ncm_FixCapacityObj;
    Object             *ncm_DebugObj;
    Object             *ncm_RemSupportObj;
    Object             *ncm_NoFallbackObj;
    Object             *ncm_PreferUasObj;
    Object             *ncm_MaxTransferObj;
    Object             *ncm_AutoDtxMaxTransObj;
    Object             *ncm_FatFSObj;
    Object             *ncm_FatDosTypeObj;
    Object             *ncm_FatControlObj;
    Object             *ncm_NTFSObj;
    Object             *ncm_NTFSDosTypeObj;
    Object             *ncm_NTFSControlObj;
    Object             *ncm_CDFSObj;
    Object             *ncm_CDDosTypeObj;
    Object             *ncm_CDControlObj;
    Object             *ncm_StartupDelayObj;
    Object             *ncm_InitialResetObj;

    Object             *ncm_LunGroupObj;
    Object             *ncm_LunLVObj;
    Object             *ncm_UnitObj;
    Object             *ncm_AutoMountLegacyObj;
    Object             *ncm_AutoMountCDObj;
    Object             *ncm_DOSNameObj;
    Object             *ncm_BuffersObj;
    Object             *ncm_MountAllLegacyObj;
    Object             *ncm_AutoMountRDBObj;
    Object             *ncm_BootObj;
    Object             *ncm_UnmountObj;

    Object             *ncm_UseObj;
    Object             *ncm_SetDefaultObj;
    Object             *ncm_CloseObj;

    Object             *ncm_AboutMI;
    Object             *ncm_UseMI;
    Object             *ncm_SetDefaultMI;
    Object             *ncm_MUIPrefsMI;

    struct Hook         ncm_LUNListDisplayHook;
};

/* Walk the live tags: the first ncm_UasQueueDepth entries of ncm_UasTags.
   Deliberately ONE plain for() rather than the usual nested-for macro trick, so
   break / continue / return / goto behave exactly as in the hand-written index
   loop it replaces (the nested form silently turns break into continue). The
   bound is re-read every iteration, just as `i < ncm->ncm_UasQueueDepth` was.
   NOT for the teardown loop in nUasDisableTags(), which must walk all
   NCM_MAXTAGS slots because the queue depth is already zero by then. */
#define MS_FOREACH_TAG(ncm, ut)                              \
    for(struct UasTag *ut = (ncm)->ncm_UasTags;              \
        ut < &(ncm)->ncm_UasTags[(ncm)->ncm_UasQueueDepth];  \
        ut++)

struct NepMSBase
{
    struct Library      nh_Library;       /* standard */
    BPTR                nh_SegList;       /* load seglist (stored by the class skeleton) */
    UWORD               nh_Flags;         /* various flags */

    struct Library     *nh_UtilityBase;   /* Utility base */

    struct NepMSDevBase *nh_DevBase;      /* base of device created */
    struct List         nh_Units;         /* List of units available */
    struct NepClassMS   nh_DummyNCM;      /* Dummy NCM for default config */

    struct SignalSemaphore nh_TaskLock;   /* single task monitor */
    struct Task        *nh_ReadySigTask;  /* Task to send ready signal to */
    LONG                nh_ReadySignal;   /* Signal to send when ready */
    struct Task        *nh_RemovableTask; /* Task for removable control */
    struct MsgPort     *nh_IOMsgPort;     /* Port for local IO */
    struct IOStdReq     nh_IOReq;         /* Fake IOReq */
    struct ExpansionBase *nh_ExpansionBase; /* ExpansionBase */
    struct Library     *nh_PsdBase;       /* PsdBase */
    struct Library     *nh_DOSBase;       /* DOS base */
    struct MsgPort     *nh_TimerMsgPort;  /* Port for timer.device */
    struct timerequest *nh_TimerIOReq;    /* Timer IO Request */
    BOOL                nh_RestartIt;     /* Restart removable task? */
};

struct NepMSDevBase
{
    struct Library      np_Library;       /* standard */
    UWORD               np_Flags;         /* various flags */

    BPTR                np_SegList;       /* device seglist */
    struct NepMSBase   *np_ClsBase;       /* pointer to class base */
    struct Library     *np_UtilityBase;   /* cached utilitybase */
};

/* ROM-safe MUI base: the config-GUI subtask (nGUITask) carries the instance `ncm` in
   tc_UserData; the shared accessor reaches ncm_MUIBase through it. See classes/mui_base.h. */
#define MUI_BASE_USERDATA struct NepClassMS
#define MUI_BASE_FIELD    ncm_MUIBase
#include "mui_base.h"

#endif /* MASSSTORAGE_H */
