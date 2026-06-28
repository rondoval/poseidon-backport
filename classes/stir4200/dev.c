/* dev.c - usbstir4200.device by Chris Hodges
*/

#include "debug.h"

#include "stir4200.class.h"

DEVBASETYPEPTR devInit(DEVBASETYPEPTR base asm("d0"), BPTR seglist asm("a0"), struct ExecBase * SysBase asm("a6"))
{

    KPRINTF(10, ("devInit base: 0x%08lx seglist: 0x%08lx SysBase: 0x%08lx\n",
                 base, seglist, SysBase));

    base->np_Library.lib_Node.ln_Type = NT_DEVICE;
    base->np_Library.lib_Node.ln_Name = DEVNAME;
    base->np_Library.lib_Flags        = LIBF_SUMUSED|LIBF_CHANGED;
    base->np_Library.lib_Version      = CLASS_VERSION;
    base->np_Library.lib_Revision     = CLASS_REVISION;
    base->np_Library.lib_IdString     = VERSION_STRING;

    /* Store segment */
    base->np_SegList = seglist;
    
    if((base->np_UtilityBase = OpenLibrary("utility.library", 0)))
    {
        KPRINTF(10, ("devInit: Ok\n"));
        KPRINTF(10, ("devInit: openCnt = %ld\n", base->np_Library.lib_OpenCnt));
        return(base);
    }
    else
    {
        return(NULL);
    }
    return(base);

}

#undef UtilityBase
#define	UtilityBase	base->np_UtilityBase

DEVBASETYPEPTR (devOpen)(struct IOIrDAReq * ioreq asm("a1"), ULONG unit asm("d0"), ULONG flags asm("d1"), struct NepSTIrDevBase * base asm("a6"))
{
    
    struct NepClassSTIr4200 *ncp;

    KPRINTF(10, ("devOpen ioreq: 0x%08lx unit: %ld flags: 0x%08lx base: 0x%08lx\n",
                 ioreq, unit, flags, base));

    ++base->np_Library.lib_OpenCnt;
    base->np_Library.lib_Flags &= ~LIBF_DELEXP;

    KPRINTF(10, ("devOpen: openCnt = %ld\n", base->np_Library.lib_OpenCnt));
    /* Damn f*cking programs which leave this field to zero! */
    if(ioreq->ioir_Req.io_Message.mn_Length && (ioreq->ioir_Req.io_Message.mn_Length < sizeof(struct IOIrDAReq)))
    {
        KPRINTF(20, ("devOpen: invalid MN_LENGTH (%ld < %ld)!\n",
                ioreq->ioir_Req.io_Message.mn_Length, sizeof(struct IOIrDAReq)));

        ioreq->ioir_Req.io_Error = IOERR_BADLENGTH;
    } else {
        /* Default to open failure. */
        ioreq->ioir_Req.io_Error = IOERR_OPENFAIL;

        ioreq->ioir_Req.io_Unit = NULL;
        ncp = (struct NepClassSTIr4200 *) base->np_ClsBase->nh_Units.lh_Head;
        while(ncp->ncp_Unit.unit_MsgPort.mp_Node.ln_Succ)
        {
            if(ncp->ncp_UnitNo == unit)
            {
                if(ncp->ncp_Unit.unit_OpenCnt)
                {
                    ioreq->ioir_Req.io_Error = IOERR_UNITBUSY;
                } else {
                    ioreq->ioir_Req.io_Unit = (struct Unit *) ncp;
                }
                break;
            }
            ncp = (struct NepClassSTIr4200 *) ncp->ncp_Unit.unit_MsgPort.mp_Node.ln_Succ;
        }

        if(!ioreq->ioir_Req.io_Unit)
        {
            ioreq->ioir_Req.io_Error = IOERR_OPENFAIL;
            KPRINTF(20, ("devOpen: could not open unit!\n"));
        } else {
            /* Opended ok! */
            ioreq->ioir_Req.io_Message.mn_Node.ln_Type = NT_REPLYMSG;
            ioreq->ioir_Req.io_Error                   = 0;
            ioreq->ioir_Req.io_Unit->unit_OpenCnt++;

            /* Allow queuing */
            ncp->ncp_DenyRequests = FALSE;

            return base;
        }
    }

    ioreq->ioir_Req.io_Unit   = (APTR) -1;
    ioreq->ioir_Req.io_Device = (APTR) -1;
    base->np_Library.lib_OpenCnt--;

    return(NULL);
    
}


BPTR (devClose)(struct IOIrDAReq * ioreq asm("a1"), struct NepSTIrDevBase * base asm("a6"))
{

    BPTR ret;
    struct NepClassSTIr4200 *ncp = (struct NepClassSTIr4200 *) ioreq->ioir_Req.io_Unit;

    KPRINTF(10, ("devClose ioreq: 0x%08lx base: 0x%08lx\n", ioreq, base));

    ret = BNULL;
    /* Try to flush the last buffer */
    Forbid();
    if(ncp->ncp_Unit.unit_MsgPort.mp_SigTask)
    {
        Signal(ncp->ncp_Unit.unit_MsgPort.mp_SigTask, 1L<<(ncp->ncp_Unit.unit_MsgPort.mp_SigBit));
    }
    Permit();

    /* Allow queuing */
    ncp->ncp_DenyRequests = FALSE;

    ncp->ncp_Unit.unit_OpenCnt--;
    ioreq->ioir_Req.io_Unit   = (APTR) -1;
    ioreq->ioir_Req.io_Device = (APTR) -1;

    if(--base->np_Library.lib_OpenCnt == 0)
    {
        if(base->np_Library.lib_Flags & LIBF_DELEXP)
        {
            KPRINTF(5, ("devClose: calling expunge...\n"));
            ret = devExpunge(base, base);
        }
    }

    KPRINTF(5, ("devClose: lib_OpenCnt = %ld\n", base->np_Library.lib_OpenCnt));

    return(ret);
    
}


BPTR (devExpunge)(DEVBASETYPEPTR extralh asm("d0"), struct NepSTIrDevBase * base asm("a6"))
{

    BPTR ret;

    KPRINTF(10, ("devExpunge base: 0x%08lx\n", base));

    ret = BNULL;

    if(base->np_Library.lib_OpenCnt == 0)
    {
        KPRINTF(5, ("devExpunge: Unloading...\n"));

        CloseLibrary(base->np_UtilityBase);

        ret = base->np_SegList;

        KPRINTF(5, ("devExpunge: removing device node 0x%08lx\n",
                    &base->np_Library.lib_Node));
        Remove(&base->np_Library.lib_Node);

        KPRINTF(5, ("devExpunge: FreeMem()...\n"));
        FreeMem((char *) base - base->np_Library.lib_NegSize,
                (ULONG) (base->np_Library.lib_NegSize + base->np_Library.lib_PosSize));

        KPRINTF(5, ("devExpunge: Unloading done! " DEVNAME " expunged!\n\n"));

        return(ret);
    }
    else
    {
        KPRINTF(5, ("devExpunge: Could not expunge, LIBF_DELEXP set!\n"));
        base->np_Library.lib_Flags |= LIBF_DELEXP;
    }

    return(BNULL);
    
}

DEVBASETYPEPTR (devReserved)(struct NepSTIrDevBase * base asm("a6"))
{
    return NULL;
}

void (devBeginIO)(struct IOIrDAReq * ioreq asm("a1"), struct NepSTIrDevBase * base asm("a6"))
{

    struct NepClassSTIr4200 *ncp = (struct NepClassSTIr4200 *) ioreq->ioir_Req.io_Unit;
    WORD ret = IOERR_NOCMD;

    KPRINTF(1, ("devBeginIO ioreq: 0x%08lx base: 0x%08lx cmd: %lu\n", ioreq, base, ioreq->ioir_Req.io_Command));

    ioreq->ioir_Req.io_Message.mn_Node.ln_Type = NT_MESSAGE;
    ioreq->ioir_Req.io_Error                   = 0;

    if(ioreq->ioir_Req.io_Command < NSCMD_DEVICEQUERY)
    {
        switch (ioreq->ioir_Req.io_Command)
        {
		    case CMD_FLUSH:
            case CMD_READ:
            case CMD_WRITE:
            case CMD_RESET:
            case CMD_CLEAR:
                if(!ncp->ncp_DenyRequests)
                {
                    ioreq->ioir_Req.io_Flags &= ~IOF_QUICK;
                    ret = RC_DONTREPLY;
                    PutMsg(&ncp->ncp_Unit.unit_MsgPort, (struct Message *) ioreq);
                } else {
                    ret = IOERR_ABORTED;
                }
                break;

            case IRCMD_QUERYDEVICE:
                ret = cmdQueryDevice(ioreq, ncp, base);
                break;

            default:
                ret = IOERR_NOCMD;
                break;
        }
    } else {
        switch(ioreq->ioir_Req.io_Command)
        {
            case NSCMD_DEVICEQUERY:
                ret = cmdNSDeviceQuery((struct IOStdReq *) ioreq, ncp, base);
                break;

            default:
                ret = IOERR_NOCMD;
                break;
        }
    }

    if(ret != RC_DONTREPLY)
    {
        KPRINTF(1, ("TermIO\n"));
        if (ret != RC_OK)
        {
            /* Set error codes
            */
            ioreq->ioir_Req.io_Error = ret & 0xff;
        }
        /* Terminate the iorequest
        */
        TermIO(ioreq, base);
    }
    
}

LONG (devAbortIO)(struct IOIrDAReq * ioreq asm("a1"), struct NepSTIrDevBase * base asm("a6"))
{

    struct NepClassSTIr4200 *ncp = (struct NepClassSTIr4200 *) ioreq->ioir_Req.io_Unit;

    struct IOIrDAReq *iocmp;

    KPRINTF(5, ("devAbortIO ioreq: 0x%08lx\n", ioreq));

    /* Is it pending?
    */
    Forbid();
    if(ioreq->ioir_Req.io_Message.mn_Node.ln_Type == NT_MESSAGE)
    {
        iocmp = (struct IOIrDAReq *) ncp->ncp_ReadQueue.lh_Head;
        while(iocmp->ioir_Req.io_Message.mn_Node.ln_Succ)
        {
            if(iocmp == ioreq)
            {
                Remove((struct Node *) ioreq);
                ioreq->ioir_Req.io_Error = IOERR_ABORTED;
                ReplyMsg(&ioreq->ioir_Req.io_Message);
                Permit();
                return(0);
            }
            iocmp = (struct IOIrDAReq *) iocmp->ioir_Req.io_Message.mn_Node.ln_Succ;
        }
        iocmp = (struct IOIrDAReq *) ncp->ncp_WriteQueue.lh_Head;
        while(iocmp->ioir_Req.io_Message.mn_Node.ln_Succ)
        {
            if(iocmp == ioreq)
            {
                Remove((struct Node *) ioreq);
                ioreq->ioir_Req.io_Error = IOERR_ABORTED;
                ReplyMsg(&ioreq->ioir_Req.io_Message);
                Permit();
                return(0);
            }
            iocmp = (struct IOIrDAReq *) iocmp->ioir_Req.io_Message.mn_Node.ln_Succ;
        }
    }
    Permit();
    return(-1);

}

/* NSD stuff
*/

static
const UWORD NSDSupported[] =
{
    CMD_RESET, CMD_CLEAR,
    CMD_FLUSH, CMD_READ,
    CMD_WRITE,
    NSCMD_DEVICEQUERY, 0
};

WORD cmdNSDeviceQuery(struct IOStdReq *ioreq,
                      struct NepClassSTIr4200 *ncp,
                      struct NepSTIrDevBase *base)
{
    struct my_NSDeviceQueryResult *query;

    query = (struct my_NSDeviceQueryResult *) ioreq->io_Data;

    KPRINTF(10, ("NSCMD_DEVICEQUERY ioreq: 0x%08lx query: 0x%08lx\n", ioreq, query));

    /* NULL ptr?
       Enough data?
       Valid request?
    */
    if((!query) ||
       (ioreq->io_Length < sizeof(struct my_NSDeviceQueryResult)) ||
       (query->DevQueryFormat != 0) ||
       (query->SizeAvailable != 0))
    {
        /* Return error. This is special handling, since iorequest is only
           guaranteed to be sizeof(struct IOIrDAReq). If we'd let our
           devBeginIO dispatcher return the error, it would trash some
           memory past end of the iorequest (ios2_WireError field).
         */
         ioreq->io_Error = IOERR_NOCMD;
         TermIO((struct IOIrDAReq *) ioreq, base);

         /* Don't reply, we already did.
         */
         return RC_DONTREPLY;
    }

    ioreq->io_Actual         = query->SizeAvailable
                             = sizeof(struct my_NSDeviceQueryResult);
    query->DeviceType        = NSDEVTYPE_UNKNOWN;
    query->DeviceSubType     = 0;
    query->SupportedCommands = NSDSupported;

    /* Return success (note that this will NOT poke ios2_WireError).
    */
    return RC_OK;
}

/* /// "cmdQueryDevice()" */
/*
 *======================================================================
 * cmdQueryDevice(ioreq, unit, base)
 *======================================================================
 *
 * This is the device UHCMD_QUERYDEVICE routine.
 *
 * Returns information about the hardware.
 *
 */

WORD cmdQueryDevice(struct IOIrDAReq *ioreq,
                    struct NepClassSTIr4200 *ncp,
                    struct NepSTIrDevBase *base)
{
    struct TagItem *taglist = (struct TagItem *) ioreq->ioir_Data;
    struct TagItem *tag;

    ULONG count = 0;

    KPRINTF(10, ("IRCMD_QUERYDEVICE ioreq: 0x%08lx, taglist: 0x%08lx\n", ioreq, taglist));

    if((tag = FindTagItem(IRA_Author, taglist)))
    {
        *((STRPTR *) tag->ti_Data) = "Chris Hodges";
        count++;
    }
    if((tag = FindTagItem(IRA_ProductName, taglist)))
    {
        *((STRPTR *) tag->ti_Data) = "STIr4200 IrDA USB Interface";
        count++;
    }
    if((tag = FindTagItem(IRA_Description, taglist)))
    {
        *((STRPTR *) tag->ti_Data) = "Interfacing device for STIr4200 IrDA sticks";
        count++;
    }
    if((tag = FindTagItem(IRA_Copyright, taglist)))
    {
        *((STRPTR *) tag->ti_Data) = "©2005-2009 Chris Hodges";
        count++;
    }
    if((tag = FindTagItem(IRA_Version, taglist)))
    {
        *((IPTR *) tag->ti_Data) = CLASS_VERSION;
        count++;
    }
    if((tag = FindTagItem(IRA_Revision, taglist)))
    {
        *((IPTR *) tag->ti_Data) = CLASS_REVISION;
        count++;
    }
    if((tag = FindTagItem(IRA_DriverVersion, taglist)))
    {
        *((IPTR *) tag->ti_Data) = 0x100;
        count++;
    }
    if((tag = FindTagItem(IRA_SuppBaudRate, taglist)))
    {
        *((IPTR *) tag->ti_Data) = BRF_2400|BRF_9600|BRF_19200|BRF_38400|BRF_57600|BRF_115200|BRF_4000000;
        count++;
    }
    if((tag = FindTagItem(IRA_SuppDataSize, taglist)))
    {
        *((IPTR *) tag->ti_Data) = 0x3f; // all sizes up to 2048
        count++;
    }
    ioreq->ioir_Actual = count;
    return RC_OK;
}
/* \\\ */

/*
 *===========================================================
 * TermIO(ioreq, base)
 *===========================================================
 *
 * Return completed ioreq to sender.
 *
 */

void TermIO(struct IOIrDAReq *ioreq,
            struct NepSTIrDevBase *base)
{
    ioreq->ioir_Req.io_Message.mn_Node.ln_Type = NT_FREEMSG;

    /* If not quick I/O, reply the message
    */
    if(!(ioreq->ioir_Req.io_Flags & IOF_QUICK))
    {
        ReplyMsg(&ioreq->ioir_Req.io_Message);
    }
}
