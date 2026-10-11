/*
 *----------------------------------------------------------------------------
 *                     usbvideo class: the video stream
 *----------------------------------------------------------------------------
 * What both transports share: finding the streaming endpoint and its
 * alternate setting, the payload engine that turns payload transfers into
 * frames, and starting and stopping a stream. The transports themselves are
 * in uvc_iso.c and uvc_bulk.c.
 *
 * Contexts: uvcStart() and uvcStop() run on the binding's subtask. The
 * engine runs where its transport does: in the host controller driver's
 * task for an isochronous stream, on the subtask for a bulk one. While a
 * stream runs the engine is the only one to touch its assembly state; the
 * read queue is shared and guarded by Forbid().
 */

#include "debug.h"
#include "usbvideo.class.h"

static const STRPTR libname = CLASS_NAME;

#define ps ncv->ncv_Base

/* /// "uvcCapacity()" */
/* What one service interval of an endpoint can carry */
static ULONG uvcCapacity(struct NepClassVideo *ncv, struct PsdEndpoint *pep)
{
    IPTR superspeed = FALSE;
    IPTR pktsize = 0;
    IPTR mult = 1;
    IPTR bytesperinterval = 0;

    psdGetAttrs(PGA_DEVICE, ncv->ncv_Device, DA_IsSuperspeed, &superspeed, TAG_END);
    psdGetAttrs(PGA_ENDPOINT, pep,
                EA_MaxPktSize, &pktsize,
                EA_NumTransMuFrame, &mult,
                EA_BytesPerInterval, &bytesperinterval,
                TAG_END);
    return(superspeed ? bytesperinterval : pktsize * mult);
}
/* \\\ */

/* /// "uvcFindAlternate()" */
/* The alternate setting to stream with, and its video endpoint (the one the
   stream's header names; an alternate may also carry a still image endpoint).
   Of the isochronous alternates it is the smallest that carries payloadsize
   bytes per service interval - no more bus bandwidth is reserved than the
   mode needs. A bulk stream has its endpoint in alternate 0 and nothing to
   choose. NULL = no alternate carries that much. */
struct PsdInterface * uvcFindAlternate(struct NepClassVideo *ncv, struct UvcStreamDesc *us,
                                       ULONG payloadsize, struct PsdEndpoint **pepptr)
{
    struct PsdInterface *pif = NULL;
    struct PsdInterface *bestif = NULL;
    ULONG bestcap = 0xffffffff;

    while((pif = psdFindInterface(ncv->ncv_Device, pif,
                                  IFA_InterfaceNum, us->us_IfNum,
                                  IFA_AlternateNum, 0xffffffff,
                                  TAG_END)))
    {
        struct PsdEndpoint *pep = psdFindEndpoint(pif, NULL,
                                                  EA_IsIn, TRUE,
                                                  EA_EndpointNum, us->us_EPAddr & 0x0f,
                                                  TAG_END);
        IPTR type = 0;

        if(!pep)
        {
            continue;
        }
        psdGetAttrs(PGA_ENDPOINT, pep, EA_TransferType, &type, TAG_END);
        if(type == USEAF_BULK)
        {
            *pepptr = pep;
            return(pif);
        }
        ULONG capacity = uvcCapacity(ncv, pep);
        if((type == USEAF_ISOCHRONOUS) && (capacity >= payloadsize) && (capacity < bestcap))
        {
            bestcap = capacity;
            bestif = pif;
            *pepptr = pep;
        }
    }
    return(bestif);
}
/* \\\ */

/* /// "uvcStreamIsBulk()" */
/* Is the stream's video endpoint a bulk one? (Else it is isochronous.) */
BOOL uvcStreamIsBulk(struct NepClassVideo *ncv, struct UvcStreamDesc *us)
{
    struct PsdEndpoint *pep = NULL;
    IPTR type = USEAF_ISOCHRONOUS;

    if(uvcFindAlternate(ncv, us, 0, &pep))
    {
        psdGetAttrs(PGA_ENDPOINT, pep, EA_TransferType, &type, TAG_END);
    }
    return(type == USEAF_BULK);
}
/* \\\ */

/*
 * ***********************************************************************
 * * The payload engine                                                  *
 * ***********************************************************************
 * Three calls per payload transfer: Begin with its first bytes, Data for
 * what follows the header, End. Two rules delimit frames, for every camera:
 *   close - the payload carries EOF, or its frame identifier differs from
 *           the open frame's, or (for the taker) the read's buffer is full
 *   open  - the first data that arrives while no frame is open
 */

/* /// "uvcDeliver()" */
/* Hand the read being filled back to its owner */
static void uvcDeliver(struct NepVideoStream *ns, LONG error, ULONG flags)
{
    struct IOStdReq *ioreq = ns->ns_Reads.ur_Cur;

    /* out of the engine before it is replied: once its owner has it back,
       AbortIO() must not find it here */
    ns->ns_Reads.ur_Cur = NULL;
    ns->ns_Reads.ur_AbortCur = FALSE;
    ioreq->io_Actual = ns->ns_Asm.ua_Fill;
    ioreq->io_Error = error;
    if(ioreq->io_Message.mn_Length >= sizeof(struct IOVideoReq))
    {
        struct IOVideoReq *ivr = (struct IOVideoReq *) ioreq;
        ivr->ivr_Sequence = ns->ns_Stats.uvs_Frames;
        ivr->ivr_Flags = flags | (ns->ns_Asm.ua_HasPTS ? IVRF_PTS : 0) | (ns->ns_Asm.ua_HasSCR ? IVRF_SCR : 0);
        ivr->ivr_PTS = ns->ns_Asm.ua_PTS;
        ivr->ivr_BusFrame = ns->ns_Asm.ua_BusFrame;
        ivr->ivr_SCRFrame = ns->ns_Asm.ua_SCRFrame;
        ivr->ivr_SCRClock = ns->ns_Asm.ua_SCRClock;
    }
    ReplyMsg(&ioreq->io_Message);
}
/* \\\ */

/* /// "uvcJpegSegment()" */
/* The segment at pos of a JPEG frame, in front of the picture data: the
   value of its length field, which counts itself and the segment's data.
   0 = there is none - the picture data begins here, or the frame is cut
   short. */
static ULONG uvcJpegSegment(const UBYTE *buf, ULONG len, ULONG pos)
{
    if((pos + JPEG_MARKER_SIZE + JPEG_LENGTH_SIZE > len) || (buf[pos] != JPEG_MARKER) || (buf[pos + 1] == JPEG_SOS))
    {
        return(0);
    }
    /* big-endian, as all of JPEG's own, and at any address */
    ULONG seglen = AROS_BE2WORD(*(const UWORD *) &buf[pos + JPEG_MARKER_SIZE]);

    return(((seglen >= JPEG_LENGTH_SIZE) && (pos + JPEG_MARKER_SIZE + seglen <= len)) ? seglen : 0);
}
/* \\\ */

/* /// "uvcEmbeddedHeader()" */
/* The first segment of an embedded stream begins with a header, followed by
   the number of bytes of the stream that this frame has
   (<devices/usb_video.h>). Gives the stream's kind and that number, and
   returns how many bytes of the segment the two take. 0 = not a valid
   header; the stream then counts as empty. */
static ULONG uvcEmbeddedHeader(const UBYTE *data, ULONG datalen, ULONG *fourcc, ULONG *size)
{
    const struct UvcxMuxHeader *mh = (const struct UvcxMuxHeader *) data;
    ULONG hdrlen = (datalen >= sizeof(*mh)) ? AROS_LE2WORD(mh->wHeaderLength) : 0;

    *fourcc = 0;
    *size = 0;
    if((hdrlen < sizeof(*mh)) || (hdrlen + sizeof(ULONG) > datalen))
    {
        return(0);
    }
    /* four characters in reading order: the one big-endian field */
    *fourcc = AROS_BE2LONG(mh->dwStreamType);
    /* behind the header, however long a later version makes it: the size */
    *size = AROS_LE2LONG(*(const ULONG *) &data[hdrlen]);
    return(hdrlen + sizeof(ULONG));
}
/* \\\ */

/* /// "uvcExtractEmbedded()" */
/* A JPEG frame into which the camera has put other streams, in application
   segments in front of the picture data (<devices/usb_video.h>). A
   stream may take several segments, and a frame may carry more than one
   stream. Moves the data of the stream of the given kind to the start of
   the buffer and returns its length; 0 = the frame carries none. */
static ULONG uvcExtractEmbedded(UBYTE *buf, ULONG len, ULONG fourcc)
{
    ULONG out = 0;
    ULONG left = 0;         /* bytes of the stream in progress that are still to come */
    BOOL wanted = FALSE;    /* it is of the kind asked for */
    ULONG seglen;

    for(ULONG pos = JPEG_MARKER_SIZE; (seglen = uvcJpegSegment(buf, len, pos)); pos += JPEG_MARKER_SIZE + seglen)
    {
        if(buf[pos + 1] != UVCX_MUX_MARKER)
        {
            continue;
        }
        const UBYTE *data = &buf[pos + JPEG_MARKER_SIZE + JPEG_LENGTH_SIZE];
        ULONG datalen = seglen - JPEG_LENGTH_SIZE;

        if(left)
        {
            /* a further segment of the stream: the stream's size counts
               this segment's marker and length too */
            left -= min(left, JPEG_MARKER_SIZE + JPEG_LENGTH_SIZE);
        } else {
            /* a stream begins */
            ULONG kind;
            ULONG skip = uvcEmbeddedHeader(data, datalen, &kind, &left);

            wanted = (kind == fourcc);
            data += skip;
            datalen -= skip;
        }
        datalen = min(datalen, left);
        left -= datalen;
        if(wanted)
        {
            /* down the buffer, over itself */
            memmove(&buf[out], data, datalen);
            out += datalen;
        }
    }
    return(out);
}
/* \\\ */

/* /// "uvcCloseFrame()" */
/* The frame in progress is complete: judge it and hand it to its read.fold 
     nobody was waiting when it began  - counted as skipped
     too long for the read's buffer    - the read ends with UVERR_OVERFLOW
     damaged                           - dropped, and its read takes the next
                                         frame; delivered with IVRF_DAMAGED if
                                         the opener asked for that
     whole                             - delivered; of a mode of H.264 inside
                                         MJPEG frames the H.264 is taken out
                                         first, and a frame that has none
                                         does not count
   Does nothing if no frame is open. */
static void uvcCloseFrame(struct NepVideoStream *ns)
{
    if(!ns->ns_Asm.ua_Open)
    {
        return;
    }
    ns->ns_Asm.ua_Open = FALSE;
    if(!ns->ns_Reads.ur_Cur)
    {
        ns->ns_Stats.uvs_Skipped++; /* no read was waiting when this frame began */
        return;
    }
    if(ns->ns_Asm.ua_Overflow)
    {
        /* the application's buffer is too small: it has to hear about it */
        ns->ns_Stats.uvs_Overflowed++;
        uvcDeliver(ns, UVERR_OVERFLOW, 0);
        return;
    }
    /* a frame joined halfway - the stream just started, or its beginning
       was lost: it does not begin as a picture does, or is not as long as one */
    const UBYTE *data = (const UBYTE *) ns->ns_Reads.ur_Cur->io_Data;
    ULONG flags = ns->ns_Mode.um_Format->ufo_Payload->up_Flags;
    BOOL isjpeg = (ns->ns_Asm.ua_Fill >= JPEG_MARKER_SIZE) && (data[0] == JPEG_MARKER) && (data[1] == JPEG_SOI);
    if(((flags & UPF_JPEGSTART) && !isjpeg) ||
       ((flags & UPF_FIXEDSIZE) && (ns->ns_Asm.ua_Fill != ns->ns_Mode.um_MaxFrameSize)))
    {
        ns->ns_Asm.ua_Damaged = TRUE;
    }
    if(!ns->ns_Asm.ua_Damaged)
    {
        if(flags & UPF_EMBEDDED)
        {
            /* what the client asked for is inside the frame: that is handed
               over, and this is the one place where the class changes what
               the camera sent. A frame with nothing inside is not a frame of
               this mode: it is not counted, so that the frames a client gets
               stay numbered without gaps, and its read takes the next. */
            ns->ns_Asm.ua_Fill = uvcExtractEmbedded((UBYTE *) ns->ns_Reads.ur_Cur->io_Data,
                                                    ns->ns_Asm.ua_Fill, UVFCC_H264);
            if(!ns->ns_Asm.ua_Fill)
            {
                ns->ns_Stats.uvs_Frames--;
                return;
            }
        }
        ns->ns_Stats.uvs_Delivered++;
        uvcDeliver(ns, 0, 0);
        return;
    }
    ns->ns_Stats.uvs_Damaged++;
    if(ns->ns_OpenFlags & UVOF_DAMAGED)
    {
        uvcDeliver(ns, 0, IVRF_DAMAGED);
    }
    /* else dropped: the read stays and takes the next frame */
}
/* \\\ */

/* /// "uvcPayloadBegin()" */
/* The start of a payload transfer. Returns the length of its header, 0 if it
   has no valid one. */
ULONG uvcPayloadBegin(struct NepVideoStream *ns, const UBYTE *hdr, ULONG len, BOOL wireerror, UWORD busframe)
{
    const struct UsbVideoPayloadHeader *ph = (const struct UsbVideoPayloadHeader *) hdr;
    ULONG hdrlen = (len >= sizeof(*ph)) ? ph->bHeaderLength : 0;

    if(ns->ns_Reads.ur_AbortCur)
    {
        /* the read being filled was called back */
        Forbid();
        if(ns->ns_Reads.ur_Cur)
        {
            ns->ns_Asm.ua_Fill = 0;
            uvcDeliver(ns, IOERR_ABORTED, 0);
        }
        ns->ns_Reads.ur_AbortCur = FALSE;
        Permit();
    }
    if(wireerror)
    {
        /* the controller reports the interval as failed or missed; it may
           have brought nothing at all (RTA_ReportInErrors) */
        ns->ns_Stats.uvs_LostIntervals++;
    }
    if((hdrlen < sizeof(*ph)) || (hdrlen > len))
    {
        if(!wireerror)
        {
            ns->ns_Stats.uvs_BadPayloads++;
        }
        ns->ns_Asm.ua_Damaged = TRUE; /* whatever this was, the open frame lost it */
        return(0);
    }
    if(ph->bmHeaderInfo & UVPHF_ERR)
    {
        ns->ns_Stats.uvs_CameraErrors++;
    }
    /* an interval the controller flagged counts like one the camera flagged */
    ns->ns_Asm.ua_PayInfo = ph->bmHeaderInfo | (wireerror ? UVPHF_ERR : 0);

    /* close: the frame identifier changed under an open frame */
    if(ns->ns_Asm.ua_Open && ((ns->ns_Asm.ua_PayInfo & UVPHF_FID) != ns->ns_Asm.ua_FID))
    {
        uvcCloseFrame(ns);
    }
    if(!ns->ns_Asm.ua_Open)
    {
        /* what a frame opened by this payload starts with: the optional
           fields follow the flags, of which a format may have more than one
           byte, in this order */
        ULONG pos = offsetof(struct UsbVideoPayloadHeader, bmHeaderInfo) + ns->ns_Mode.um_Format->ufo_Payload->up_InfoBytes;
        const ULONG *pts = (const ULONG *) &hdr[pos];

        ns->ns_Asm.ua_BusFrame = busframe;
        ns->ns_Asm.ua_HasPTS = (ns->ns_Asm.ua_PayInfo & UVPHF_PTS) && (hdrlen >= pos + sizeof(*pts));
        ns->ns_Asm.ua_PTS = ns->ns_Asm.ua_HasPTS ? AROS_LE2LONG(*pts) : 0;
        pos += (ns->ns_Asm.ua_PayInfo & UVPHF_PTS) ? sizeof(*pts) : 0;
        const struct UsbVideoSCR *scr = (const struct UsbVideoSCR *) &hdr[pos];
        ns->ns_Asm.ua_HasSCR = (ns->ns_Asm.ua_PayInfo & UVPHF_SCR) && (hdrlen >= pos + sizeof(*scr));
        ns->ns_Asm.ua_SCRClock = ns->ns_Asm.ua_HasSCR ? AROS_LE2LONG(scr->dwSourceClock) : 0;
        ns->ns_Asm.ua_SCRFrame = ns->ns_Asm.ua_HasSCR ? (AROS_LE2WORD(scr->wSOFCounter) & UVPH_SCR_FRAME_MASK) : 0;
    }
    return(hdrlen);
}
/* \\\ */

/* /// "uvcPayloadData()" */
/* The data of a payload transfer, behind its header. The first data while
   no frame is open opens one and takes the next waiting read for it. What
   does not fit the read's buffer is dropped, and the frame remembers it. */
void uvcPayloadData(struct NepVideoStream *ns, const UBYTE *data, ULONG len)
{
    struct IOStdReq *ioreq;

    if(!len)
    {
        return;
    }
    if(!ns->ns_Asm.ua_Open)
    {
        /* open: the first data while no frame is open */
        ns->ns_Asm.ua_Open = TRUE;
        ns->ns_Asm.ua_FID = ns->ns_Asm.ua_PayInfo & UVPHF_FID;
        ns->ns_Stats.uvs_Frames++;
        ns->ns_Asm.ua_Fill = 0;
        ns->ns_Asm.ua_Damaged = FALSE;
        ns->ns_Asm.ua_Overflow = FALSE;
        if(!ns->ns_Reads.ur_Cur)
        {
            Forbid();
            ns->ns_Reads.ur_Cur = (struct IOStdReq *) RemHead(&ns->ns_Reads.ur_Queue);
            Permit();
        }
    }
    if(!(ioreq = ns->ns_Reads.ur_Cur))
    {
        return;
    }
    if(len > ioreq->io_Length - ns->ns_Asm.ua_Fill)
    {
        ns->ns_Asm.ua_Overflow = TRUE;
        len = ioreq->io_Length - ns->ns_Asm.ua_Fill;
    }
    CopyMem((APTR) data, (UBYTE *) ioreq->io_Data + ns->ns_Asm.ua_Fill, len);
    ns->ns_Asm.ua_Fill += len;
}
/* \\\ */

/* /// "uvcPayloadEnd()" */
/* The end of a payload transfer: an error it was flagged with damages the
   frame, and EOF closes the frame. */
void uvcPayloadEnd(struct NepVideoStream *ns)
{
    if(ns->ns_Asm.ua_PayInfo & UVPHF_ERR)
    {
        ns->ns_Asm.ua_Damaged = TRUE;
    }
    /* close: end of frame */
    if(ns->ns_Asm.ua_PayInfo & UVPHF_EOF)
    {
        uvcCloseFrame(ns);
    }
}
/* \\\ */

/*
 * ***********************************************************************
 * * Start and stop                                                      *
 * ***********************************************************************
 * What a transport does differently is in its own file: uvcIsoStart() and
 * uvcIsoStop(), uvcBulkStart() and uvcBulkStop().
 */

/* /// "uvcIsConnected()" */
/* A camera that was unplugged has nothing left to talk to */
BOOL uvcIsConnected(struct NepClassVideo *ncv)
{
    IPTR connected = FALSE;

    psdGetAttrs(PGA_DEVICE, ncv->ncv_Device, DA_IsConnected, &connected, TAG_END);
    return(connected != 0);
}
/* \\\ */

/* /// "uvcStart()" */
/* Commit the negotiated mode, take the bandwidth, start the transport.
   Returns 0 or a UVERR_ / IOERR_ code. */
LONG uvcStart(struct NepClassVideo *ncv, struct NepVideoStream *ns)
{
    struct UvcStreamDesc *us = ns->ns_Desc;
    UWORD version = ncv->ncv_Function->uf_Version;
    ULONG size = (version >= UVC_VERSION_15) ? UVPC_SIZE_15 :
                 ((version >= UVC_VERSION_11) ? UVPC_SIZE_11 : UVPC_SIZE_10);
    struct PsdEndpoint *pep = NULL;
    struct PsdInterface *altif;
    LONG ioerr;

    if(ns->ns_Streaming)
    {
        return(UVERR_BUSY);
    }
    if(!ns->ns_Mode.um_Format)
    {
        return(UVERR_NOMODE);
    }

    /* a mode of H.264 inside the frames: the camera's H.264 unit is told
       again, for it forgets at the end of every stream */
    if((ns->ns_Mode.um_Format->ufo_Payload->up_Flags & UPF_EMBEDDED) &&
       (ioerr = uvcEmbedCommit(ncv, &ns->ns_Mode.um_Embed)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "The camera's H.264 unit did not take its configuration: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        return((ioerr == UHIOERR_STALL) ? UVERR_REFUSED : UVERR_USB);
    }
    /* the block goes back as the camera sent it at negotiation */
    if((ioerr = uvcRequest(ncv, UVUDR_SET_CUR, 0, us->us_IfNum, UVVSCS_COMMIT_CONTROL,
                           &ns->ns_Mode.um_Probe, size, NULL)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "The camera did not accept the mode: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        return((ioerr == UHIOERR_STALL) ? UVERR_REFUSED : UVERR_USB);
    }
    /* a bulk stream has the one setting it is in and takes no bandwidth:
       nothing to select, and a camera may refuse a request it has no use for */
    if(!(altif = uvcFindAlternate(ncv, us, ns->ns_Mode.um_PayloadSize, &pep)) ||
       (!ns->ns_IsBulk && !psdSetAltInterface(ncv->ncv_EP0Pipe, altif)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "No bus bandwidth for %ld bytes per interval.", ns->ns_Mode.um_PayloadSize);
        return(UVERR_NOBANDWIDTH);
    }

    /* one service interval, or one bulk transfer */
    ns->ns_ScratchSize = ns->ns_IsBulk ? uvcBulkBufferSize(ns) : uvcCapacity(ncv, pep);
    if((ns->ns_Scratch = psdAllocVec(ns->ns_ScratchSize)))
    {
        ns->ns_Asm.ua_Open = FALSE;
        ns->ns_Reads.ur_Cur = NULL;
        ns->ns_Reads.ur_AbortCur = FALSE;
        /* frames are numbered, and counted, from here; readers
           (NSCMD_UV_GETSTATS) look under Forbid() */
        Forbid();
        memset(&ns->ns_Stats, 0, sizeof(ns->ns_Stats));
        Permit();
        if(!(ioerr = ns->ns_IsBulk ? uvcBulkStart(ncv, ns, pep) : uvcIsoStart(ncv, ns, pep)))
        {
            ns->ns_Streaming = TRUE;
            return(0);
        }
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "Could not start the stream: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        psdFreeVec(ns->ns_Scratch);
        ns->ns_Scratch = NULL;
    }
    if(!ns->ns_IsBulk)
    {
        uvcZeroBandwidth(ncv, us);
    }
    return(UVERR_USB);
}
/* \\\ */

/* /// "uvcStop()" */
/* Stop the transport and leave the unit ready for the next start: reads
   stay queued, and the one being filled goes back to the head of the queue.
   Does nothing if no stream runs. */
void uvcStop(struct NepClassVideo *ncv, struct NepVideoStream *ns)
{
    if(!ns->ns_Streaming)
    {
        return;
    }
    /* the engine does not run after this */
    if(ns->ns_IsBulk)
    {
        uvcBulkStop(ncv, ns);
    } else {
        uvcIsoStop(ncv, ns);
    }

    /* The frame in progress will never be completed: its read goes back to
       the head of the queue for the next start, unless it was called back. */
    Forbid();
    if(ns->ns_Reads.ur_Cur)
    {
        if(ns->ns_Reads.ur_AbortCur)
        {
            ns->ns_Asm.ua_Fill = 0;
            uvcDeliver(ns, IOERR_ABORTED, 0);
        } else {
            AddHead(&ns->ns_Reads.ur_Queue, &ns->ns_Reads.ur_Cur->io_Message.mn_Node);
            ns->ns_Reads.ur_Cur = NULL;
        }
    }
    ns->ns_Reads.ur_AbortCur = FALSE;
    ns->ns_Asm.ua_Open = FALSE;
    ns->ns_Streaming = FALSE;
    Permit();

    psdFreeVec(ns->ns_Scratch);
    ns->ns_Scratch = NULL;
}
/* \\\ */

/* /// "uvcAbortReads()" */
/* Reply every waiting read with the given error. A read the engine is
   filling right now is called back and replied by the engine. Callable from
   any context. */
void uvcAbortReads(struct NepVideoStream *ns, LONG error)
{
    struct IOStdReq *ioreq;

    Forbid();
    while((ioreq = (struct IOStdReq *) RemHead(&ns->ns_Reads.ur_Queue)))
    {
        ioreq->io_Actual = 0;
        ioreq->io_Error = error;
        ReplyMsg(&ioreq->io_Message);
    }
    if(ns->ns_Reads.ur_Cur)
    {
        ns->ns_Reads.ur_AbortCur = TRUE;
    }
    Permit();
}
/* \\\ */
