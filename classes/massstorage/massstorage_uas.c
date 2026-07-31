/*
 *----------------------------------------------------------------------------
 *                         massstorage class for poseidon
 *----------------------------------------------------------------------------
 *                   By Chris Hodges <chrisly@platon42.de>
 */

#include "debug.h"

#include "massstorage.class.h"

extern const STRPTR libname;

#undef  ps
#define ps ncm->ncm_Base

/* Retire a tag (see below): also the release path of the synchronous
   transport, which owns a tag but no client request. */
static void nUasFinalizeTag(struct NepClassMS *ncm, struct UasTag *ut, LONG ioerr);

static void nUasFillLun(UBYTE *lun, UWORD lunnum)
{
    memset(lun, 0, 8);
    lun[0] = (UBYTE) (lunnum & 0x3f);
}

static inline BOOL nUasErrForgiven(LONG ioerr)
{
    return (ioerr == 0) || nIsOverflowErr(ioerr) || (ioerr == UHIOERR_RUNTPACKET);
}

/* Pick the Sense IU apart: SCSI status at byte 6, inline sense data behind
   the 16-byte header. Callers preinitialize *status / *iu_id / *sense_actual
   to their defaults; short IUs leave them untouched.
   Only a Sense IU carries a SCSI status - byte 6 of any other IU means
   something else entirely (in a Response IU it is additional response info),
   so callers must check *iu_id before trusting *status. */
static void nUasParseStatusIU(const UBYTE *statusbuf, ULONG actual_len,
                              UBYTE *status, UBYTE *iu_id,
                              UBYTE *sense_data, ULONG sense_len, UWORD *sense_actual)
{
    UBYTE id = actual_len ? statusbuf[0] : 0;

    if(iu_id)
    {
        *iu_id = id;
    }
    if(status && (id == UAS_IU_ID_SENSE) &&
       (actual_len > offsetof(struct UasSenseIU, iu_Status)))
    {
        *status = ((const struct UasSenseIU *) statusbuf)->iu_Status;
    }
    if((id == UAS_IU_ID_SENSE) && sense_data && sense_actual)
    {
        ULONG header = offsetof(struct UasSenseIU, iu_Sense);

        if(actual_len > header)
        {
            const struct UasSenseIU *senseiu = (const struct UasSenseIU *) statusbuf;
            ULONG sense_avail = actual_len - header;
            ULONG copy_len = AROS_BE2WORD(senseiu->iu_Length);

            if(copy_len > sense_avail)
            {
                copy_len = sense_avail;
            }
            if(copy_len > sense_len)
            {
                copy_len = sense_len;
            }
            if(copy_len)
            {
                CopyMem((APTR) &statusbuf[header], sense_data, copy_len);
                *sense_actual = (UWORD) copy_len;
            }
        }
    }
}

/* Claim a tag for a synchronous command, waiting for one to free up and
   keeping the engine turning meanwhile. Runs in the unit task: every
   nScsiDirect() path either is the unit task or reaches it through
   nIOCmdTunnel(), so waiting on the task port here is always the right port
   and the right task. NULL only when the engine is down. */
static struct UasTag * nUasClaimTag(struct NepClassMS *ncm)
{
    struct UasTag *ut;

    if(!ncm->ncm_UasQueueDepth)
    {
        return NULL;
    }
    while(!(ut = nUasFreeTag(ncm)))
    {
        /* completions free tags; the TMF ladder frees quarantined ones */
        nUasProcessAborts(ncm);
        nUasReapTags(ncm);
        nUasProcessQuarantine(ncm);
        if((ut = nUasFreeTag(ncm)))
        {
            break;
        }
        /* The quarantine ladder can escalate to a device reset, and a rebuild
           that fails leaves no engine at all - with no tags there is nothing
           left to signal us, so bail instead of waiting forever. */
        if(!ncm->ncm_UasQueueDepth || ncm->ncm_DenyRequests)
        {
            return NULL;
        }
        Wait(1L<<ncm->ncm_TaskMsgPort->mp_SigBit);
    }
    if(!ut->ut_StatusPipe)
    {
        return NULL; /* engine half-built (a post-reset rebuild failed) */
    }

    /* Claim it: the FIFO dispatcher and the reaper both key on the state, so
       a RUNNING tag with no ioreq is ours alone for the duration. */
    ut->ut_IOReq = NULL; /* no client request - the caller blocks instead */
    ut->ut_State = UTS_RUNNING;
    ut->ut_StatusArmed = FALSE;
    ut->ut_DataArmed = FALSE;
    ut->ut_Outstanding = 0;
    ut->ut_Offset = 0;
    ut->ut_AbortReq = FALSE;
    ut->ut_Failed = FALSE;
    ut->ut_CmdSent = FALSE;
    ut->ut_StatusSeen = FALSE;
    return ut;
}

/*
 * One synchronous SCSI command on a tag of its own.
 *
 * The multi-step handlers (geometry, mode pages, the unaligned/emulated block
 * paths, every nScsiDirect() caller) run here. Taking a free tag rather than
 * draining the engine is what makes them barrier-free: sibling tags keep
 * their data moving while this command is on the wire, and psdWaitPipe()
 * only ever consumes its own pipe's reply, leaving the others queued on the
 * shared port for the next reap.
 *
 * A failure that leaves the DEVICE owning the command quarantines the tag on
 * release, exactly like the async path.
 */
static LONG nUasDoCommand(struct NepClassMS *ncm, const UBYTE *cdb, UWORD cdb_len,
                           UBYTE *data, ULONG data_len, BOOL read,
                           ULONG *actual, UBYTE *status, UBYTE *iu_id,
                           UBYTE *sense_data, ULONG sense_len, UWORD *sense_actual)
{
    struct UasCommandIU cmdiu;
    struct UasTag *ut;
    struct PsdPipe *statuspipe;
    const char *phase;
    ULONG cmdlen;
    ULONG actual_len;
    LONG ioerr;

    if(actual)
    {
        *actual = 0;
    }
    if(status)
    {
        *status = SCSI_GOOD;
    }
    if(iu_id)
    {
        *iu_id = 0;
    }
    if(sense_actual)
    {
        *sense_actual = 0;
    }

    if(!(ut = nUasClaimTag(ncm)))
    {
        /* no engine (a post-reset rebuild failed): no transport left */
        return HFERR_Phase;
    }
    statuspipe = ut->ut_StatusPipe;

    memset(&cmdiu, 0, sizeof(cmdiu));
    cmdiu.iu_Id = UAS_IU_ID_COMMAND;
    cmdiu.iu_TaskAttr = 0;
    cmdiu.iu_Tag = AROS_WORD2BE(ut->ut_Tag);
    nUasFillLun(cmdiu.iu_Lun, ncm->ncm_UnitLUN);
    cmdlen = (cdb_len > 16) ? 16 : cdb_len;
    if(cmdlen)
    {
        CopyMem(cdb, cmdiu.iu_Cdb, cmdlen);
    }

    /* Pre-post the Status IU read before the command/data phases. UAS gives
       status its own endpoint precisely so the host read can already be
       outstanding when the device delivers status; posting it up front lets it
       complete alongside the data phase instead of costing a separate host
       round-trip afterwards. It is reaped below, or aborted and reclaimed on
       any early-out - the tag's buffer is never left armed behind us. */
    psdSendPipe(statuspipe, ut->ut_StatusBuf, sizeof(ut->ut_StatusBuf));

    KPRINTF(5, ("UAS sync cmd tag %ld op 0x%02lx dlen %ld\n",
                (ULONG) ut->ut_Tag, (ULONG) (cmdlen ? cdb[0] : 0), data_len));
    ioerr = psdDoPipe(ncm->ncm_EPCmdPipe, &cmdiu, sizeof(cmdiu));
    if(ioerr)
    {
        phase = "command IU";
        goto fail_reclaim_status;
    }
    ut->ut_CmdSent = TRUE; /* the device owns this tag until its Sense IU */

    if(data_len)
    {
        struct PsdPipe *pp = read ? ut->ut_DataInPipe : ut->ut_DataOutPipe;

        ioerr = psdDoPipe(pp, data, data_len);
        if(actual)
        {
            *actual = psdGetPipeActual(pp);
        }
        if(!nUasErrForgiven(ioerr))
        {
            phase = "data";
            goto fail_reclaim_status;
        }
    }

    ioerr = psdWaitPipe(statuspipe);
    if(!nUasErrForgiven(ioerr))
    {
        KPRINTF(10, ("UAS sync status IU failed: %ld\n", ioerr));
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS status IU transfer failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        nUasFinalizeTag(ncm, ut, 0); /* no ioreq: releases or quarantines */
        return ioerr;
    }
    actual_len = psdGetPipeActual(statuspipe);
    KPRINTF(5, ("UAS sync status IU id 0x%02lx actual %ld\n",
                (ULONG) (actual_len ? ut->ut_StatusBuf[0] : 0), actual_len));
    if(actual_len && (ut->ut_StatusBuf[0] == UAS_IU_ID_SENSE))
    {
        ut->ut_StatusSeen = TRUE; /* the device closed the command itself */
    }
    nUasParseStatusIU(ut->ut_StatusBuf, actual_len, status, iu_id,
                      sense_data, sense_len, sense_actual);
    if(!ut->ut_StatusSeen)
    {
        /* Only a Sense IU closes a command. Anything else here (a Response
           IU, a READY IU we never negotiated, an empty read) is a protocol
           violation, not a status - and leaves the tag quarantined. */
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS unexpected status IU 0x%02lx (%ld bytes)",
                       (ULONG) (actual_len ? ut->ut_StatusBuf[0] : 0), actual_len);
        nUasFinalizeTag(ncm, ut, 0);
        return HFERR_Phase;
    }
    nUasFinalizeTag(ncm, ut, 0);
    return 0;

fail_reclaim_status:
    KPRINTF(10, ("UAS sync %s failed: %ld\n", phase, ioerr));
    psdAbortPipe(statuspipe);
    psdWaitPipe(statuspipe);
    psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                   "UAS %s transfer failed: %s (%ld)",
                   (STRPTR) phase, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
    nUasFinalizeTag(ncm, ut, 0);
    return ioerr;
}

/* /// "nScsiDirectUAS()" */
LONG nScsiDirectUAS(struct NepClassMS *ncm, struct SCSICmd *scsicmd)
{
    LONG ioerr;
    LONG rioerr = 0;
    ULONG datalen;
    UBYTE status = SCSI_GOOD;
    UBYTE iu_id = 0;

    datalen = scsicmd->scsi_Length;
    scsicmd->scsi_Status = SCSI_GOOD;
    scsicmd->scsi_Actual = 0;
    scsicmd->scsi_CmdActual = (scsicmd->scsi_CmdLength > 16) ? 16 : scsicmd->scsi_CmdLength;
    scsicmd->scsi_SenseActual = 0;

    nLockXFer(ncm);
    do
    {
        if(ncm->ncm_DenyRequests)
        {
            rioerr = HFERR_Phase;
            break;
        }

        ioerr = nUasDoCommand(ncm, scsicmd->scsi_Command, scsicmd->scsi_CmdLength,
                              (UBYTE *) scsicmd->scsi_Data, datalen,
                              (scsicmd->scsi_Flags & SCSIF_READ) != 0,
                              &scsicmd->scsi_Actual, &status, &iu_id,
                              (scsicmd->scsi_Flags & SCSIF_AUTOSENSE) ? scsicmd->scsi_SenseData : NULL,
                              scsicmd->scsi_SenseLength, &scsicmd->scsi_SenseActual);
        if(ioerr)
        {
            /* nUasDoCommand already reported the failing phase */
            scsicmd->scsi_Status = SCSI_CHECK_CONDITION;
            rioerr = HFERR_Phase;
            break;
        }

        scsicmd->scsi_Status = status;
        if(status)
        {
            rioerr = HFERR_BadStatus;
            if((scsicmd->scsi_Flags & SCSIF_AUTOSENSE) && (!scsicmd->scsi_SenseActual))
            {
                UBYTE sensecmd[6];
                UBYTE sense_status = SCSI_GOOD;
                ULONG sense_actual = 0;

                sensecmd[0] = SCSI_REQUEST_SENSE;
                sensecmd[1] = 0;
                sensecmd[2] = 0;
                sensecmd[3] = 0;
                sensecmd[4] = (UBYTE) scsicmd->scsi_SenseLength;
                sensecmd[5] = 0;

                ioerr = nUasDoCommand(ncm, sensecmd, 6,
                                      scsicmd->scsi_SenseData, scsicmd->scsi_SenseLength,
                                      TRUE, &sense_actual, &sense_status, NULL,
                                      NULL, 0, NULL);
                if(!ioerr)
                {
                    scsicmd->scsi_SenseActual = (UWORD) sense_actual;
                } else {
                    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                   "UAS request sense failed: %s (%ld)",
                                   psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                }
            }
        }

    } while(FALSE);
    nUnlockXFer(ncm);
    return(rioerr);
}
/* \\ */

/*
 * UAS multi-tag engine: N concurrent tagged commands, one per stream id.
 * The unit task owns every tag - submits from the FIFO dispatcher, reaps
 * completions off ncm_TaskMsgPort (psdCheckPipe/psdWaitPipe keep the pipe
 * bookkeeping intact), chunks large transfers on the same tag, and drains
 * to empty before any synchronous (barrier) command runs.
 */

static inline struct PsdPipe * nUasTagDataPipe(struct UasTag *ut)
{
    return ut->ut_IsRead ? ut->ut_DataInPipe : ut->ut_DataOutPipe;
}

/* Blocking cleanup of a tag's armed pipes (task context; used when the
   command IU itself failed, so nothing rides the wire anymore). */
static void nUasTagAbortArmed(struct NepClassMS *ncm, struct UasTag *ut)
{
    if(ut->ut_StatusArmed)
    {
        psdAbortPipe(ut->ut_StatusPipe);
        psdWaitPipe(ut->ut_StatusPipe);
        ut->ut_StatusArmed = FALSE;
        ut->ut_Outstanding--;
    }
    if(ut->ut_DataArmed)
    {
        psdAbortPipe(nUasTagDataPipe(ut));
        psdWaitPipe(nUasTagDataPipe(ut));
        ut->ut_DataArmed = FALSE;
        ut->ut_Outstanding--;
    }
}

/* Send one chunk: arm the status pipe, then the data pipe, command IU last
   (Linux uas ordering). The command IU is a sub-ms message on its own
   non-stream endpoint - blocking on it keeps IU ordering trivial. */
static LONG nUasSubmitChunk(struct NepClassMS *ncm, struct UasTag *ut)
{
    ULONG maxtrans = 1UL<<(ncm->ncm_CDC->cdc_MaxTransfer+16);
    UBYTE *cdb = ut->ut_CmdIU.iu_Cdb;
    ULONG datalen = ut->ut_Remain;
    LONG ioerr;

    if(datalen > maxtrans)
    {
        datalen = maxtrans;
    }
    ut->ut_ChunkLen = datalen;

    memset(&ut->ut_CmdIU, 0, sizeof(ut->ut_CmdIU));
    ut->ut_CmdIU.iu_Id = UAS_IU_ID_COMMAND;
    ut->ut_CmdIU.iu_Tag = AROS_WORD2BE(ut->ut_Tag);
    nUasFillLun(ut->ut_CmdIU.iu_Lun, ncm->ncm_UnitLUN);

    nBuildRWCdb(cdb, !ut->ut_IsRead, ut->ut_StartBlockHigh, ut->ut_StartBlock,
                datalen, ncm->ncm_BlockShift);

    KPRINTF(5, ("UAS tag %ld submit LBA %ld:%ld len %ld\n",
                (ULONG) ut->ut_Tag, ut->ut_StartBlockHigh, ut->ut_StartBlock, datalen));
    psdSendPipe(ut->ut_StatusPipe, ut->ut_StatusBuf, sizeof(ut->ut_StatusBuf));
    ut->ut_StatusArmed = TRUE;
    ut->ut_Outstanding = 1;
    if(datalen)
    {
        psdSendPipe(nUasTagDataPipe(ut), ut->ut_Data + ut->ut_Offset, datalen);
        ut->ut_DataArmed = TRUE;
        ut->ut_Outstanding++;
    }

    /* fresh chunk: the device owns nothing of it until the Command IU lands */
    ut->ut_CmdSent = FALSE;
    ut->ut_StatusSeen = FALSE;

    ioerr = psdDoPipe(ncm->ncm_EPCmdPipe, &ut->ut_CmdIU, sizeof(ut->ut_CmdIU));
    if(ioerr)
    {
        nUasTagAbortArmed(ncm, ut);
    } else {
        ut->ut_CmdSent = TRUE; /* the device owns this tag until its Sense IU */
    }
    return ioerr;
}

/* Retire a tag's request. A tag killed host-side while the DEVICE still owns
   its command (Command IU delivered, no Sense IU back: NAK timeout, AbortIO,
   any pipe error) does not become reusable here - it goes to quarantine and
   only the ABORT TASK TMF (or the reset escalation) frees it. Reusing it
   would let the old command's Sense IU land in the new command's status
   transfer. */
static void nUasFinalizeTag(struct NepClassMS *ncm, struct UasTag *ut, LONG ioerr)
{
    struct IOStdReq *ioreq = ut->ut_IOReq;
    BOOL quarantine = ut->ut_CmdSent && !ut->ut_StatusSeen;

    /* clear the request link first: devAbortIO matches on it under Forbid */
    ut->ut_IOReq = NULL;
    ut->ut_State = quarantine ? UTS_QUARANTINE : UTS_FREE;
    if(quarantine)
    {
        KPRINTF(10, ("UAS tag %ld quarantined: device still owns the command\n",
                     (ULONG) ut->ut_Tag));
    }
    if(ioreq)
    {
        ioreq->io_Error = ioerr;
        ioreq->io_Actual = ut->ut_Offset;
        ReplyMsg((struct Message *) ioreq);
    }
}

/* One pipe of a tag replied: collect it, and once the chunk is fully reaped
   either finalize, or advance to the next chunk on the same tag. */
static void nUasTagPipeDone(struct NepClassMS *ncm, struct UasTag *ut,
                            struct PsdPipe *pp, BOOL is_status)
{
    LONG ioerr = psdWaitPipe(pp); /* already replied: result + bookkeeping only */

    if(is_status)
    {
        ut->ut_StatusArmed = FALSE;
        if(nUasErrForgiven(ioerr))
        {
            /* Only a Sense IU closes the command device-side; anything else
               (or a failed read) leaves the device owning the tag. */
            ULONG len = psdGetPipeActual(pp);

            ut->ut_StatusSeen = len && (ut->ut_StatusBuf[0] == UAS_IU_ID_SENSE);
        }
    } else {
        ut->ut_DataArmed = FALSE;
    }
    ut->ut_Outstanding--;

    if(!nUasErrForgiven(ioerr) && !ut->ut_Failed)
    {
        KPRINTF(10, ("UAS tag %ld %s pipe failed: %ld\n",
                     (ULONG) ut->ut_Tag, is_status ? "status" : "data", ioerr));
        ut->ut_Failed = TRUE;
        ut->ut_IOErr = ioerr;
        if(!ut->ut_AbortReq)
        {
            psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                           "UAS tag %ld %s pipe failed: %s (%ld)",
                           (ULONG) ut->ut_Tag, is_status ? "status" : "data",
                           psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        }
        /* the sibling can't complete on its own anymore - abort it; its
           reply is reaped like any other and lands in the failed finalize */
        if(ut->ut_StatusArmed)
        {
            psdAbortPipe(ut->ut_StatusPipe);
        }
        if(ut->ut_DataArmed)
        {
            psdAbortPipe(nUasTagDataPipe(ut));
        }
    }

    if(ut->ut_Outstanding)
    {
        return;
    }

    /* chunk complete */
    if(ut->ut_Failed)
    {
        nUasFinalizeTag(ncm, ut, ut->ut_AbortReq ? IOERR_ABORTED : HFERR_Phase);
        return;
    }

    UBYTE scsistatus = SCSI_GOOD;
    UBYTE iu_id = 0;
    nUasParseStatusIU(ut->ut_StatusBuf, psdGetPipeActual(ut->ut_StatusPipe),
                      &scsistatus, &iu_id, NULL, 0, NULL);
    if(iu_id != UAS_IU_ID_SENSE)
    {
        /* Not a status at all (a Response IU, a READY IU we never negotiated,
           an empty read): the command's device-side fate is unknown, so
           ut_StatusSeen stayed clear and the finalize quarantines the tag. */
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS tag %ld unexpected status IU 0x%02lx",
                       (ULONG) ut->ut_Tag, (ULONG) iu_id);
        nUasFinalizeTag(ncm, ut, HFERR_Phase);
        return;
    }
    if(scsistatus != SCSI_GOOD)
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "UAS tag %ld bad SCSI status %ld (IU 0x%02lx)",
                       (ULONG) ut->ut_Tag, (ULONG) scsistatus, (ULONG) iu_id);
        nUasFinalizeTag(ncm, ut, HFERR_BadStatus);
        return;
    }

    ut->ut_Offset += ut->ut_ChunkLen;
    ut->ut_Remain -= ut->ut_ChunkLen;
    if(ut->ut_Remain)
    {
        ULONG oldstartblock = ut->ut_StartBlock;

        ut->ut_StartBlock += ut->ut_ChunkLen >> ncm->ncm_BlockShift;
        if(ut->ut_StartBlock < oldstartblock)
        {
            // wrap around occurred
            ut->ut_StartBlockHigh++;
        }
        if(nUasSubmitChunk(ncm, ut))
        {
            nUasFinalizeTag(ncm, ut, HFERR_Phase);
        }
        return;
    }

    nUasFinalizeTag(ncm, ut, 0);
}

struct UasTag * nUasFreeTag(struct NepClassMS *ncm)
{
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[i];

        if(ut->ut_State != UTS_FREE)
        {
            continue; /* in flight, or quarantined until its TMF is answered */
        }
        if(ut->ut_StatusPipe == ncm->ncm_UasTMArmed)
        {
            continue; /* borrow mode: this tag's status pipe carries the TMF */
        }
        return ut;
    }
    return NULL;
}

/* Async-eligible = the plain block-IO family on a known-good geometry;
   everything else (and every alignment/emulation special case) stays on the
   synchronous barrier path exactly as before. */
BOOL nUasEligible(struct NepClassMS *ncm, struct IOStdReq *ioreq)
{
    if(!ncm->ncm_UasQueueDepth)
    {
        return FALSE;
    }
    switch(ioreq->io_Command)
    {
        case CMD_READ:
        case NSCMD_TD_READ64:
        case TD_READ64:
        case CMD_WRITE:
        case NSCMD_TD_WRITE64:
        case TD_WRITE64:
            break;

        default:
            return FALSE;
    }
    if(!ncm->ncm_BlockSize)
    {
        return FALSE; /* the first block IO probes the block size synchronously */
    }
    if(ncm->ncm_CDC->cdc_PatchFlags & PFF_EMUL_LARGE_BLK)
    {
        return FALSE;
    }
    if(!ioreq->io_Length)
    {
        return FALSE;
    }
    if((((ioreq->io_Length >> ncm->ncm_BlockShift)<<ncm->ncm_BlockShift) != ioreq->io_Length) ||
       (((ioreq->io_Offset >> ncm->ncm_BlockShift)<<ncm->ncm_BlockShift) != ioreq->io_Offset))
    {
        return FALSE; /* unaligned: the sync path runs its emul-fallback logic */
    }
    return TRUE;
}

/* Bind an eligible request to a free tag and put its first chunk on the
   wire. The request is consumed either way: in flight, or replied here. */
void nUasSubmitTag(struct NepClassMS *ncm, struct UasTag *ut, struct IOStdReq *ioreq)
{
    switch(ioreq->io_Command)
    {
        case CMD_READ:
            ioreq->io_Actual = 0; /* 32-bit command: io_Actual is not an offset */
        case NSCMD_TD_READ64:
        case TD_READ64:
            ut->ut_IsRead = TRUE;
            break;

        case CMD_WRITE:
            ioreq->io_Actual = 0;
        default:
            ut->ut_IsRead = FALSE;
            break;
    }

    ut->ut_IOReq = ioreq;
    ut->ut_Data = (UBYTE *) ioreq->io_Data;
    ut->ut_Offset = 0;
    ut->ut_Remain = ioreq->io_Length;
    ut->ut_StartBlockHigh = ioreq->io_Actual>>ncm->ncm_BlockShift;
    ut->ut_StartBlock = (ioreq->io_Offset>>ncm->ncm_BlockShift)|(ioreq->io_Actual<<(32-ncm->ncm_BlockShift));
    ut->ut_AbortReq = FALSE;
    ut->ut_Failed = FALSE;
    ut->ut_IOErr = 0;
    ut->ut_State = UTS_RUNNING;

    LONG ioerr = nUasSubmitChunk(ncm, ut);
    if(ioerr)
    {
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS tag %ld command IU failed: %s (%ld)",
                       (ULONG) ut->ut_Tag,
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        nUasFinalizeTag(ncm, ut, HFERR_Phase);
    }
}

/* devAbortIO flagged tags (ut_AbortReq) get their pipes aborted here, in the
   task's context; the replies then take the ordinary failed-reap path. */
void nUasProcessAborts(struct NepClassMS *ncm)
{
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[i];

        /* only a RUNNING tag owns a request and armed pipes; a quarantined
           one is already retired and waits on its TMF */
        if((ut->ut_State != UTS_RUNNING) || !ut->ut_AbortReq)
        {
            continue;
        }
        if(ut->ut_StatusArmed)
        {
            psdAbortPipe(ut->ut_StatusPipe);
        }
        if(ut->ut_DataArmed)
        {
            psdAbortPipe(nUasTagDataPipe(ut));
        }
    }
}

/* Collect every replied engine pipe (non-blocking). psdCheckPipe spots the
   replies without disturbing the port, psdWaitPipe (in nUasTagPipeDone)
   collects them with full bookkeeping. */
void nUasReapTags(struct NepClassMS *ncm)
{
    BOOL progress;

    do
    {
        progress = FALSE;
        for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
        {
            struct UasTag *ut = &ncm->ncm_UasTags[i];

            if(ut->ut_State != UTS_RUNNING)
            {
                continue; /* only a running tag has armed pipes */
            }
            if(ut->ut_StatusArmed && psdCheckPipe(ut->ut_StatusPipe))
            {
                nUasTagPipeDone(ncm, ut, ut->ut_StatusPipe, TRUE);
                progress = TRUE;
            }
            if(ut->ut_State != UTS_RUNNING)
            {
                continue; /* finalized above (free or quarantined) */
            }
            if(ut->ut_DataArmed && psdCheckPipe(nUasTagDataPipe(ut)))
            {
                nUasTagPipeDone(ncm, ut, nUasTagDataPipe(ut), FALSE);
                progress = TRUE;
            }
        }
    } while(progress);
}

/*
 * Task Management: releasing a quarantined tag.
 *
 * A tag killed host-side while the device still owns its command cannot be
 * reused until the device is told to drop that task - otherwise the old
 * command's Sense IU lands in the reused tag's fresh status transfer. The
 * ladder is: ABORT TASK TMF -> (on refusal/timeout) device reset -> (if the
 * stack cannot reset) degrade to the pre-quarantine behaviour.
 */

/* Blocking kill of every tag in flight, ahead of a device reset (which
   retires the device-side task set anyway, so nothing stays quarantined). */
static void nUasKillAllTags(struct NepClassMS *ncm)
{
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[i];

        if(ut->ut_State != UTS_RUNNING)
        {
            continue;
        }
        nUasTagAbortArmed(ncm, ut);
        ut->ut_Failed = TRUE;
        ut->ut_CmdSent = FALSE; /* the reset settles the device side */
        nUasFinalizeTag(ncm, ut, IOERR_ABORTED);
    }
}

/* The escalation: reset the device and rebuild the engine on fresh endpoint
   contexts. Degrades (never wedges) when the stack cannot reset. */
static void nUasEscalateReset(struct NepClassMS *ncm)
{
    if(ncm->ncm_UasTMArmed)
    {
        psdAbortPipe(ncm->ncm_UasTMArmed);
        psdWaitPipe(ncm->ncm_UasTMArmed);
        ncm->ncm_UasTMArmed = NULL;
    }
    ncm->ncm_UasTMTargetTag = 0;

    /* psdResetDevice's contract: the caller owns quiescing its own traffic */
    nUasKillAllTags(ncm);

    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                   "UAS task management failed; resetting device.");

    if(psdResetDevice(ncm->ncm_Device))
    {
        /* Endpoint contexts and stream rings were rebuilt from scratch and
           the library invalidated the endpoint tokens and stream
           bookkeeping, so re-arming re-allocates real rings. */
        nUasDisableTags(ncm);
        if(!nUasInitTags(ncm))
        {
            psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                           "UAS tag engine could not be rebuilt after reset.");
            ncm->ncm_DenyRequests = TRUE;
        }
        return;
    }

    /* No reset available (an HCD or library without the op) or it failed:
       fall back to the pre-quarantine behaviour - release the tags and accept
       the stale-Sense-IU hazard rather than wedging the unit for good. */
    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                   "Device reset unavailable; releasing quarantined UAS tags.");
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[i];

        if(ut->ut_State == UTS_QUARANTINE)
        {
            ut->ut_State = UTS_FREE;
            ut->ut_CmdSent = FALSE;
            ut->ut_StatusSeen = FALSE;
        }
    }
}

static struct UasTag * nUasQuarantinedTag(struct NepClassMS *ncm)
{
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        if(ncm->ncm_UasTags[i].ut_State == UTS_QUARANTINE)
        {
            return &ncm->ncm_UasTags[i];
        }
    }
    return NULL;
}

/* Send ABORT TASK for the first quarantined tag. One TMF at a time; further
   quarantined tags queue behind it. */
static void nUasIssueTMF(struct NepClassMS *ncm)
{
    struct UasTaskMgmtIU tmiu;
    struct UasTag *target;
    struct PsdPipe *tmpipe;
    UWORD owntag;
    LONG ioerr;

    if(ncm->ncm_UasTMArmed || !(target = nUasQuarantinedTag(ncm)))
    {
        return;
    }

    if(ncm->ncm_UasTMTag)
    {
        tmpipe = ncm->ncm_UasTMStatusPipe;
        owntag = ncm->ncm_UasTMTag;
    } else {
        /* Borrow mode: the TM IU needs a tag of its own, and it must not
           collide with a task the device still knows about - so every other
           tag has to be quiet before we can lend one out. */
        struct UasTag *lend = NULL;

        for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
        {
            struct UasTag *ut = &ncm->ncm_UasTags[i];

            if(ut == target)
            {
                continue;
            }
            if(ut->ut_State != UTS_FREE)
            {
                return; /* still busy: retried once it drains */
            }
            if(!lend)
            {
                lend = ut;
            }
        }
        if(!lend)
        {
            /* queue depth 1: no second tag to carry the TM IU */
            nUasEscalateReset(ncm);
            return;
        }
        tmpipe = lend->ut_StatusPipe;
        owntag = lend->ut_Tag;
    }

    memset(&tmiu, 0, sizeof(tmiu));
    tmiu.iu_Id = UAS_IU_ID_TASK_MGMT;
    tmiu.iu_Tag = AROS_WORD2BE(owntag);
    tmiu.iu_Function = UAS_TMF_ABORT_TASK;
    tmiu.iu_TaskTag = AROS_WORD2BE(target->ut_Tag);
    nUasFillLun(tmiu.iu_Lun, ncm->ncm_UnitLUN);

    KPRINTF(10, ("UAS ABORT TASK for tag %ld (TM tag %ld)\n",
                 (ULONG) target->ut_Tag, (ULONG) owntag));

    /* the Response IU arrives on the TM tag's status stream: arm it first */
    psdSendPipe(tmpipe, ncm->ncm_UasTMBuf, sizeof(ncm->ncm_UasTMBuf));
    ioerr = psdDoPipe(ncm->ncm_EPCmdPipe, &tmiu, sizeof(tmiu));
    if(ioerr)
    {
        psdAbortPipe(tmpipe);
        psdWaitPipe(tmpipe);
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS ABORT TASK IU failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        nUasEscalateReset(ncm);
        return;
    }

    ncm->ncm_UasTMArmed = tmpipe;
    ncm->ncm_UasTMOwnTag = owntag;
    ncm->ncm_UasTMTargetTag = target->ut_Tag;
}

/* Collect the Response IU (non-blocking). The TM pipe's NAK timeout is the
   TMF deadline - no separate timer. */
static void nUasReapTMF(struct NepClassMS *ncm)
{
    const struct UasResponseIU *resp;
    ULONG actual;
    LONG ioerr;
    UBYTE code;

    if(!ncm->ncm_UasTMArmed || !psdCheckPipe(ncm->ncm_UasTMArmed))
    {
        return;
    }
    ioerr = psdWaitPipe(ncm->ncm_UasTMArmed);
    actual = psdGetPipeActual(ncm->ncm_UasTMArmed);
    ncm->ncm_UasTMArmed = NULL;

    if(!nUasErrForgiven(ioerr))
    {
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS ABORT TASK for tag %ld failed: %s (%ld)",
                       (ULONG) ncm->ncm_UasTMTargetTag,
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        nUasEscalateReset(ncm);
        return;
    }

    resp = (const struct UasResponseIU *) ncm->ncm_UasTMBuf;
    code = (actual > offsetof(struct UasResponseIU, iu_Response))
           ? resp->iu_Response : 0xff;
    if((ncm->ncm_UasTMBuf[0] != UAS_IU_ID_RESPONSE) ||
       (actual < sizeof(struct UasResponseIU)) ||
       (AROS_BE2WORD(resp->iu_Tag) != ncm->ncm_UasTMOwnTag) ||
       ((code != UAS_RESP_TMF_COMPLETE) && (code != UAS_RESP_TMF_SUCCEEDED)))
    {
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "UAS ABORT TASK for tag %ld refused (IU 0x%02lx, response 0x%02lx)",
                       (ULONG) ncm->ncm_UasTMTargetTag,
                       (ULONG) (actual ? ncm->ncm_UasTMBuf[0] : 0), (ULONG) code);
        nUasEscalateReset(ncm);
        return;
    }

    /* the device dropped the task: the tag is reusable again */
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[i];

        if((ut->ut_Tag == ncm->ncm_UasTMTargetTag) && (ut->ut_State == UTS_QUARANTINE))
        {
            ut->ut_State = UTS_FREE;
            ut->ut_CmdSent = FALSE;
            ut->ut_StatusSeen = FALSE;
            break;
        }
    }
    KPRINTF(10, ("UAS tag %ld released by ABORT TASK\n",
                 (ULONG) ncm->ncm_UasTMTargetTag));
    ncm->ncm_UasTMTargetTag = 0;
}

/* Drive the quarantine state machine: collect a finished TMF, start the next
   one. Runs in the unit task, from the main loop and from the drain. */
void nUasProcessQuarantine(struct NepClassMS *ncm)
{
    if(!ncm->ncm_UasQueueDepth)
    {
        return;
    }
    nUasReapTMF(ncm);
    nUasIssueTMF(ncm);
}

BOOL nUasTagsIdle(struct NepClassMS *ncm)
{
    for(UWORD i = 0; i < ncm->ncm_UasQueueDepth; i++)
    {
        /* a quarantined tag owns no request but is NOT reusable: the drain
           and the suspend probe must both keep waiting for the TMF */
        if(ncm->ncm_UasTags[i].ut_State != UTS_FREE)
        {
            return FALSE;
        }
    }
    return TRUE;
}

/* The barrier: block until every tag has completed. Every synchronous
   command (and the task teardown) runs behind this. Quarantined tags count as
   busy, so the drain also drives the TMF ladder - bounded by the TM pipe's
   NAK timeout and, past that, by the reset escalation. */
void nUasDrainTags(struct NepClassMS *ncm)
{
    if(!ncm->ncm_UasQueueDepth)
    {
        return;
    }
    for(;;)
    {
        nUasProcessAborts(ncm);
        nUasReapTags(ncm);
        nUasProcessQuarantine(ncm);
        if(nUasTagsIdle(ncm))
        {
            break;
        }
        Wait(1L<<ncm->ncm_TaskMsgPort->mp_SigBit);
    }
}

BOOL nUasCollectEndpoints(struct NepClassMS *ncm)
{
    struct PsdDescriptor *pdd = NULL;
    struct PsdEndpoint *pep;
    UBYTE *data;
    IPTR  dtype;
    IPTR  len;
    IPTR  eptype;
    IPTR  is_in;
    UBYTE pipe_id;

    /* the unit struct is reused across replugs - drop the previous binding's
       (freed) endpoint objects or the guards below keep them forever */
    ncm->ncm_EPCmd = NULL;
    ncm->ncm_EPStatus = NULL;
    ncm->ncm_EPIn = NULL;
    ncm->ncm_EPOut = NULL;

    while((pdd = psdFindDescriptor(ncm->ncm_Device, pdd,
                                   DDA_Interface, ncm->ncm_Interface,
                                   TAG_END)))
    {
        psdGetAttrs(PGA_DESCRIPTOR, pdd,
                    DDA_DescriptorType, &dtype,
                    DDA_DescriptorData, &data,
                    DDA_DescriptorLength, &len,
                    DDA_Endpoint, &pep,
                    TAG_END);
        if((!pep) || (!data) || (len < 3))
        {
            continue;
        }
        if((dtype != UAS_DESC_PIPE_USAGE) && (dtype != UAS_DESC_CS_ENDPOINT))
        {
            continue;
        }
        pipe_id = 0;
        if(dtype == UAS_DESC_PIPE_USAGE)
        {
            pipe_id = data[2];
        } else if(len >= 4) {
            pipe_id = data[3];
        }
        if(!pipe_id)
        {
            continue;
        }
        psdGetAttrs(PGA_ENDPOINT, pep,
                    EA_TransferType, &eptype,
                    EA_IsIn, &is_in,
                    TAG_END);
        if(eptype != USEAF_BULK)
        {
            continue;
        }
        switch(pipe_id)
        {
            case UAS_PIPE_ID_COMMAND:
                if(is_in)
                {
                    break;
                }
                if(!ncm->ncm_EPCmd)
                {
                    ncm->ncm_EPCmd = pep;
                }
                break;
            case UAS_PIPE_ID_STATUS:
                if(!is_in)
                {
                    break;
                }
                if(!ncm->ncm_EPStatus)
                {
                    ncm->ncm_EPStatus = pep;
                }
                break;
            case UAS_PIPE_ID_DATA_IN:
                if(!is_in)
                {
                    break;
                }
                if(!ncm->ncm_EPIn)
                {
                    ncm->ncm_EPIn = pep;
                }
                break;
            case UAS_PIPE_ID_DATA_OUT:
                if(is_in)
                {
                    break;
                }
                if(!ncm->ncm_EPOut)
                {
                    ncm->ncm_EPOut = pep;
                }
                break;
        }
    }

    return ncm->ncm_EPCmd && ncm->ncm_EPStatus && ncm->ncm_EPIn && ncm->ncm_EPOut;
}

void nUasDisableTags(struct NepClassMS *ncm)
{
    /* callers drain first: no tag owns a request here */
    if(ncm->ncm_UasTMStatusPipe)
    {
        psdSetAttrs(PGA_PIPE, ncm->ncm_UasTMStatusPipe, PPA_StreamID, 0, TAG_END);
        psdFreePipe(ncm->ncm_UasTMStatusPipe);
        ncm->ncm_UasTMStatusPipe = NULL;
    }
    ncm->ncm_UasTMArmed = NULL;
    ncm->ncm_UasTMTag = 0;
    ncm->ncm_UasTMOwnTag = 0;
    ncm->ncm_UasTMTargetTag = 0;

    for(UWORD i = 0; i < NCM_MAXTAGS; i++)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[i];

        if(ut->ut_StatusPipe)
        {
            /* dropping the stream id frees the HCD's rings (first hit per
               endpoint; the rest are no-ops) */
            psdSetAttrs(PGA_PIPE, ut->ut_StatusPipe, PPA_StreamID, 0, TAG_END);
            psdFreePipe(ut->ut_StatusPipe);
            ut->ut_StatusPipe = NULL;
        }
        if(ut->ut_DataInPipe)
        {
            psdSetAttrs(PGA_PIPE, ut->ut_DataInPipe, PPA_StreamID, 0, TAG_END);
            psdFreePipe(ut->ut_DataInPipe);
            ut->ut_DataInPipe = NULL;
        }
        if(ut->ut_DataOutPipe)
        {
            psdSetAttrs(PGA_PIPE, ut->ut_DataOutPipe, PPA_StreamID, 0, TAG_END);
            psdFreePipe(ut->ut_DataOutPipe);
            ut->ut_DataOutPipe = NULL;
        }
        ut->ut_State = UTS_FREE;
        ut->ut_IOReq = NULL;
        ut->ut_StatusArmed = FALSE;
        ut->ut_DataArmed = FALSE;
        ut->ut_Outstanding = 0;
    }
    ncm->ncm_UasQueueDepth = 0;
}

/* Bring up the tag engine. Per-tag pipes carry
   PPA_StreamID == tag, assigned in descending order so the library's stream
   allocation happens once per endpoint instead of a free+realloc per pipe.
   The configured queue depth is clamped to 1..NCM_MAXTAGS below, so a stored
   chunk from an older build cannot overrun the tag array. Invariants at entry:
   ncm_MaxLUN == 0 (UAS is LUN 0 only - GET_MAX_LUN is a BOT request and is
   skipped for UAS), all four endpoints present (nUasCollectEndpoints failed
   the bind otherwise).
   FALSE = engine unavailable; the caller fails the bind loudly. */
BOOL nUasInitTags(struct NepClassMS *ncm)
{
    IPTR maxstreams_in = 0;
    IPTR maxstreams_out = 0;
    IPTR maxstreams_status = 0;
    ULONG qd = ncm->ncm_CDC->cdc_UasQueueDepth;

    ncm->ncm_UasQueueDepth = 0;
    memset(ncm->ncm_UasTags, 0, sizeof(ncm->ncm_UasTags));
    ncm->ncm_UasTMStatusPipe = NULL;
    ncm->ncm_UasTMArmed = NULL;
    ncm->ncm_UasTMTag = 0;
    ncm->ncm_UasTMOwnTag = 0;
    ncm->ncm_UasTMTargetTag = 0;

    if(qd < 1)
    {
        qd = 1;
    }
    else if(qd > NCM_MAXTAGS)
    {
        qd = NCM_MAXTAGS;
    }

    psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPIn,
                EA_MaxStreams, &maxstreams_in,
                TAG_END);
    psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPOut,
                EA_MaxStreams, &maxstreams_out,
                TAG_END);
    psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPStatus,
                EA_MaxStreams, &maxstreams_status,
                TAG_END);
    KPRINTF(10, ("UAS init tags: qd %ld, MaxStreams in/out/status %ld/%ld/%ld\n",
                 qd, maxstreams_in, maxstreams_out, maxstreams_status));
    if(!maxstreams_in || !maxstreams_out || !maxstreams_status)
    {
        psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                       "UAS tag engine: no stream capability (MaxStreams %ld/%ld/%ld).",
                       maxstreams_in, maxstreams_out, maxstreams_status);
        return FALSE;
    }
    if(qd > maxstreams_in)
    {
        qd = maxstreams_in;
    }
    if(qd > maxstreams_out)
    {
        qd = maxstreams_out;
    }
    if(qd > maxstreams_status)
    {
        qd = maxstreams_status;
    }

    /* Reserve stream id QD+1 on the status endpoint for Task Management when
       the device has the headroom: the TM IU carries a tag of its own and its
       Response IU comes back on that tag's status stream, so it must not
       collide with a live command tag. Allocated FIRST, because the first
       PPA_StreamID on an endpoint sizes its ring set (growing later is a
       free+realloc the library only performs on an idle endpoint).
       Without headroom the engine falls back to borrow mode: drain the other
       tags and lend the TMF one of their tags. */
    if(maxstreams_status > qd)
    {
        ncm->ncm_UasTMStatusPipe = psdAllocPipe(ncm->ncm_Device, ncm->ncm_TaskMsgPort,
                                                ncm->ncm_EPStatus);
        if(ncm->ncm_UasTMStatusPipe)
        {
            IPTR alloc_tm = 0;

            psdSetAttrs(PGA_PIPE, ncm->ncm_UasTMStatusPipe,
                        PPA_StreamID, qd + 1,
                        PPA_AllowRuntPackets, TRUE,
                        TAG_END);
            nSetNakTimeout(ncm, ncm->ncm_UasTMStatusPipe, ncm->ncm_CDC->cdc_NakTimeout*100);

            psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPStatus, EA_StreamsAlloc, &alloc_tm, TAG_END);
            if(alloc_tm > qd)
            {
                ncm->ncm_UasTMTag = (UWORD) (qd + 1);
            } else {
                /* no ring for the extra id - drop back to borrow mode. Safe
                   to clear here (it releases the endpoint's rings): the tag
                   pipes below have not claimed any yet. */
                psdSetAttrs(PGA_PIPE, ncm->ncm_UasTMStatusPipe, PPA_StreamID, 0, TAG_END);
                psdFreePipe(ncm->ncm_UasTMStatusPipe);
                ncm->ncm_UasTMStatusPipe = NULL;
            }
        }
    }

    for(ULONG tag = qd; tag >= 1; tag--)
    {
        struct UasTag *ut = &ncm->ncm_UasTags[tag - 1];

        ut->ut_Tag = (UWORD) tag;
        ut->ut_StatusPipe = psdAllocPipe(ncm->ncm_Device, ncm->ncm_TaskMsgPort, ncm->ncm_EPStatus);
        ut->ut_DataInPipe = psdAllocPipe(ncm->ncm_Device, ncm->ncm_TaskMsgPort, ncm->ncm_EPIn);
        ut->ut_DataOutPipe = psdAllocPipe(ncm->ncm_Device, ncm->ncm_TaskMsgPort, ncm->ncm_EPOut);
        if(!ut->ut_StatusPipe || !ut->ut_DataInPipe || !ut->ut_DataOutPipe)
        {
            psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                           "UAS tag engine: pipe allocation failed.");
            nUasDisableTags(ncm);
            return FALSE;
        }
        psdSetAttrs(PGA_PIPE, ut->ut_StatusPipe,
                    PPA_StreamID, tag,
                    PPA_AllowRuntPackets, TRUE,
                    TAG_END);
        psdSetAttrs(PGA_PIPE, ut->ut_DataInPipe,
                    PPA_StreamID, tag,
                    TAG_END);
        psdSetAttrs(PGA_PIPE, ut->ut_DataOutPipe,
                    PPA_StreamID, tag,
                    PPA_NoShortPackets, TRUE,
                    TAG_END);
        nSetNakTimeout(ncm, ut->ut_StatusPipe, ncm->ncm_CDC->cdc_NakTimeout*100);
        nSetNakTimeout(ncm, ut->ut_DataInPipe, ncm->ncm_CDC->cdc_NakTimeout*100);
        nSetNakTimeout(ncm, ut->ut_DataOutPipe, ncm->ncm_CDC->cdc_NakTimeout*100);
    }

    /* The library stays silently single-ring when ALLOC_STREAMS fails - at
       queue depth > 1 that would interleave tags on one ring and corrupt
       data, so verify the rings really exist. */
    {
        IPTR alloc_in = 0;
        IPTR alloc_out = 0;
        IPTR alloc_status = 0;

        psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPIn, EA_StreamsAlloc, &alloc_in, TAG_END);
        psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPOut, EA_StreamsAlloc, &alloc_out, TAG_END);
        psdGetAttrs(PGA_ENDPOINT, ncm->ncm_EPStatus, EA_StreamsAlloc, &alloc_status, TAG_END);
        KPRINTF(10, ("UAS init tags: StreamsAlloc in/out/status %ld/%ld/%ld\n",
                     alloc_in, alloc_out, alloc_status));
        if((alloc_in < qd) || (alloc_out < qd) || (alloc_status < qd))
        {
            psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                           "UAS tag engine: stream rings not allocated (%ld/%ld/%ld < %ld).",
                           alloc_in, alloc_out, alloc_status, qd);
            nUasDisableTags(ncm);
            return FALSE;
        }
    }

    ncm->ncm_UasQueueDepth = (UWORD) qd;
    psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                   "UAS tag engine enabled at queue depth %ld (task management on %s).",
                   qd, ncm->ncm_UasTMTag ? (STRPTR) "its own stream"
                                         : (STRPTR) "a borrowed tag");
    return TRUE;
}
