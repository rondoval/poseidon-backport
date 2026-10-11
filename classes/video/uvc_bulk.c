/*
 *----------------------------------------------------------------------------
 *                   usbvideo class: the bulk transport
 *----------------------------------------------------------------------------
 * One bulk transfer of the camera = one payload transfer: it begins with
 * the payload header and ends with a short packet, or when it has the
 * negotiated size. The class takes it in pieces of at most UVC_BULK_CHUNK
 * bytes, one transfer in flight at a time. While the subtask talks to the
 * camera's controls the stream waits; on a bulk endpoint the camera is held
 * off by that, not overrun.
 *
 * Contexts: everything here runs on the binding's subtask - the transfers
 * come back to its message port - and so does the payload engine
 * (uvc_stream.c) for a bulk stream. uvcBulkStart() and uvcBulkStop() are
 * called by uvcStart() and uvcStop().
 */

#include "debug.h"
#include "usbvideo.class.h"

static const STRPTR libname = CLASS_NAME;

#define ps ncv->ncv_Base

#define UVC_BULK_CHUNK      32768 /* the most one transfer asks for */
#define UVC_BULK_MAX_ERRORS 8     /* failed transfers in a row after which no more is asked for */

/* /// "uvcBulkBufferSize()" */
/* The scratch buffer of a bulk stream holds one transfer: a whole payload,
   if that is no longer than a chunk */
ULONG uvcBulkBufferSize(struct NepVideoStream *ns)
{
    ULONG payloadsize = ns->ns_Mode.um_PayloadSize;

    return((payloadsize && (payloadsize < UVC_BULK_CHUNK)) ? payloadsize : UVC_BULK_CHUNK);
}
/* \\\ */

/* /// "uvcBulkAsk()" */
/* What the next transfer asks for: the rest of the payload in progress, as
   far as the buffer goes. Never more than the rest - no short packet ends a
   payload of the full size, and the next one begins right behind it. A
   camera that names no size gets whole buffers: its payloads end with short
   packets only. */
static ULONG uvcBulkAsk(struct NepVideoStream *ns)
{
    ULONG left = ns->ns_Mode.um_PayloadSize - ns->ns_Bulk.ub_PayDone;

    return((ns->ns_Mode.um_PayloadSize && (left < ns->ns_ScratchSize)) ? left : ns->ns_ScratchSize);
}
/* \\\ */

/* /// "uvcBulkTransfer()" */
/* A transfer came back with len bytes in the scratch buffer, or failed.
   Feeds the engine and returns what the next transfer asks for; 0 = the
   endpoint keeps failing and nothing more is asked for. No calls but the
   engine's: this is the whole of the bulk logic. */
static ULONG uvcBulkTransfer(struct NepVideoStream *ns, LONG ioerr, ULONG len)
{
    ULONG asked = uvcBulkAsk(ns);
    ULONG hdrlen = 0;

    if(ioerr)
    {
        /* what it brought is of no use, and where the payload ends is not
           known any more: the frame is damaged, as by a lost interval, and
           the next transfer is taken for the start of a payload */
        uvcPayloadBegin(ns, ns->ns_Scratch, 0, TRUE, 0);
        ns->ns_Bulk.ub_PayDone = 0;
        return((++ns->ns_Bulk.ub_Errors < UVC_BULK_MAX_ERRORS) ? uvcBulkAsk(ns) : 0);
    }
    ns->ns_Bulk.ub_Errors = 0;
    if(!ns->ns_Bulk.ub_PayDone)
    {
        if(!len)
        {
            /* nothing, between two payloads: a camera may end one of the
               full size like that */
            return(asked);
        }
        hdrlen = uvcPayloadBegin(ns, ns->ns_Scratch, len, FALSE, 0);
        ns->ns_Bulk.ub_PaySkip = !hdrlen;
    }
    if(!ns->ns_Bulk.ub_PaySkip)
    {
        uvcPayloadData(ns, ns->ns_Scratch + hdrlen, len - hdrlen);
    }
    ns->ns_Bulk.ub_PayDone += len;
    if((len < asked) || (ns->ns_Bulk.ub_PayDone == ns->ns_Mode.um_PayloadSize))
    {
        if(!ns->ns_Bulk.ub_PaySkip)
        {
            uvcPayloadEnd(ns);
        }
        ns->ns_Bulk.ub_PayDone = 0;
    }
    return(uvcBulkAsk(ns));
}
/* \\\ */

/* /// "uvcBulkDone()" */
/* The transfer of a bulk stream came back to the subtask */
void uvcBulkDone(struct NepClassVideo *ncv, struct NepVideoStream *ns)
{
    LONG ioerr = psdGetPipeError(ns->ns_Bulk.ub_Pipe);
    ULONG ask = uvcBulkTransfer(ns, ioerr, psdGetPipeActual(ns->ns_Bulk.ub_Pipe));

    if(ask)
    {
        psdSendPipe(ns->ns_Bulk.ub_Pipe, ns->ns_Scratch, ask);
        return;
    }
    /* the reads wait, as for a camera that has fallen silent */
    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                   "Unit %ld: the stream keeps failing: %s (%ld). Nothing more is read until it is stopped.",
                   ns->ns_UnitNo, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
}
/* \\\ */

/* /// "uvcBulkStart()" */
/* Returns the USB error. */
LONG uvcBulkStart(struct NepClassVideo *ncv, struct NepVideoStream *ns, struct PsdEndpoint *pep)
{
    if(!(ns->ns_Bulk.ub_Pipe = psdAllocPipe(ncv->ncv_Device, ncv->ncv_TaskMsgPort, pep)))
    {
        return(UHIOERR_OUTOFMEMORY);
    }
    /* a payload is as long as it is, and a camera with nothing to send - a
       grabber without a source - may take as long as it likes */
    psdSetAttrs(PGA_PIPE, ns->ns_Bulk.ub_Pipe,
                PPA_AllowRuntPackets, TRUE,
                PPA_NakTimeout, FALSE,
                TAG_END);
    ns->ns_Bulk.ub_PayDone = 0;
    ns->ns_Bulk.ub_PaySkip = FALSE;
    ns->ns_Bulk.ub_Errors = 0;
    psdSendPipe(ns->ns_Bulk.ub_Pipe, ns->ns_Scratch, uvcBulkAsk(ns));
    return(0);
}
/* \\\ */

/* /// "uvcBulkStop()" */
/* Call the transfer in flight back and tell the camera that the stream has
   ended */
void uvcBulkStop(struct NepClassVideo *ncv, struct NepVideoStream *ns)
{
    /* the transfer in flight is called back and collected here: it does not
       reach the subtask's loop, and nothing comes after it */
    psdAbortPipe(ns->ns_Bulk.ub_Pipe);
    psdWaitPipe(ns->ns_Bulk.ub_Pipe);
    psdFreePipe(ns->ns_Bulk.ub_Pipe);
    ns->ns_Bulk.ub_Pipe = NULL;

    /* A bulk stream has no setting to go back to and the specification
       names no way to end one. Clearing the endpoint's halt is what hosts
       do: it is how the camera learns that nobody is reading any more. */
    if(uvcIsConnected(ncv))
    {
        LONG ioerr = psdClearEndpointHalt(ncv->ncv_EP0Pipe, ns->ns_Desc->us_EPAddr);
        if(ioerr)
        {
            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                           "The camera did not take the end of the stream: %s (%ld)",
                           psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        }
    }
}
/* \\\ */
