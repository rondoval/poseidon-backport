/*
 *----------------------------------------------------------------------------
 *                usbvideo class: the isochronous transport
 *----------------------------------------------------------------------------
 * One hook pair per service interval that carried data = one payload
 * transfer. The controller driver copies the interval into the scratch
 * buffer between the two hooks.
 *
 * Contexts: the hooks, and with them the payload engine (uvc_stream.c), run
 * in the host controller driver's task, once per service interval that
 * carried data, and must not block. uvcIsoStart() and uvcIsoStop() run on
 * the binding's subtask, called by uvcStart() and uvcStop().
 */

#include "debug.h"
#include "usbvideo.class.h"

static const STRPTR libname = CLASS_NAME;

#define ps ncv->ncv_Base

/* /// "uvcInReqHook()" */
/* Before a service interval is read: where the controller driver is to put it */
static void uvcInReqHook(struct Hook *hook asm("a0"), APTR urti asm("a2"), struct IOUsbHWBufferReq *ubr asm("a1"))
{
    struct NepVideoStream *ns = (struct NepVideoStream *) hook->h_Data;

    ubr->ubr_Buffer = ns->ns_Scratch;
    if(ubr->ubr_Length > ns->ns_ScratchSize)
    {
        ubr->ubr_Length = ns->ns_ScratchSize;
    }
}
/* \\\ */

/* /// "uvcInDoneHook()" */
/* A service interval has been read, or was lost: one payload transfer for
   the engine */
static void uvcInDoneHook(struct Hook *hook asm("a0"), APTR urti asm("a2"), struct IOUsbHWBufferReq *ubr asm("a1"))
{
    struct NepVideoStream *ns = (struct NepVideoStream *) hook->h_Data;
    ULONG hdrlen = uvcPayloadBegin(ns, ns->ns_Scratch, ubr->ubr_Length,
                                   (ubr->ubr_Flags & UHCD_UBF_XFER_ERROR) != 0, ubr->ubr_Frame);

    if(hdrlen)
    {
        uvcPayloadData(ns, ns->ns_Scratch + hdrlen, ubr->ubr_Length - hdrlen);
        uvcPayloadEnd(ns);
    }
}
/* \\\ */

/* /// "uvcZeroBandwidth()" */
/* Back to alternate 0: the stream's bus bandwidth is given back */
void uvcZeroBandwidth(struct NepClassVideo *ncv, struct UvcStreamDesc *us)
{
    struct PsdInterface *pif = psdFindInterface(ncv->ncv_Device, NULL,
                                                IFA_InterfaceNum, us->us_IfNum,
                                                IFA_AlternateNum, 0,
                                                TAG_END);
    if(pif && !psdSetAltInterface(ncv->ncv_EP0Pipe, pif))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "Could not return interface %ld to its idle setting.", (ULONG) us->us_IfNum);
    }
}
/* \\\ */

/* /// "uvcIsoStart()" */
/* The alternate is selected. Returns the USB error. */
LONG uvcIsoStart(struct NepClassVideo *ncv, struct NepVideoStream *ns, struct PsdEndpoint *pep)
{
    ns->ns_Iso.ui_ReqHook.h_Entry = (APTR) uvcInReqHook;
    ns->ns_Iso.ui_ReqHook.h_Data = ns;
    ns->ns_Iso.ui_DoneHook.h_Entry = (APTR) uvcInDoneHook;
    ns->ns_Iso.ui_DoneHook.h_Data = ns;
    /* an interval that is lost must not go unnoticed: it damages its frame */
    if(!(ns->ns_Iso.ui_Handler = psdAllocRTIsoHandler(pep,
                                             RTA_InRequestHook, &ns->ns_Iso.ui_ReqHook,
                                             RTA_InDoneHook, &ns->ns_Iso.ui_DoneHook,
                                             RTA_ReportInErrors, TRUE,
                                             TAG_END)))
    {
        return(UHIOERR_OUTOFMEMORY);
    }
    LONG ioerr = psdStartRTIso(ns->ns_Iso.ui_Handler);
    if(ioerr)
    {
        psdFreeRTIsoHandler(ns->ns_Iso.ui_Handler);
        ns->ns_Iso.ui_Handler = NULL;
    }
    return(ioerr);
}
/* \\\ */

/* /// "uvcIsoStop()" */
/* Stop the hooks and give the bus bandwidth back */
void uvcIsoStop(struct NepClassVideo *ncv, struct NepVideoStream *ns)
{
    /* returns when the endpoint has drained: no hook runs after this */
    psdStopRTIso(ns->ns_Iso.ui_Handler);
    psdFreeRTIsoHandler(ns->ns_Iso.ui_Handler);
    ns->ns_Iso.ui_Handler = NULL;
    if(uvcIsConnected(ncv))
    {
        uvcZeroBandwidth(ncv, ns->ns_Desc);
    }
}
/* \\\ */
