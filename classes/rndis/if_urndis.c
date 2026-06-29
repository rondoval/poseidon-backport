/*
 * $Id$
 */

//#define DEBUG 1
#include "debug.h"
#include "rndis.class.h"

#define htole32(x) AROS_LONG2LE(x)
#define letoh32(x) AROS_LE2LONG(x)

#undef  ps
#define ps ncp->ncp_Base


uint32_t urndis_ctrl_msg(struct NepClassEth *ncp, uint8_t rt, uint8_t r,
    uint16_t index, uint16_t value, void *buf, size_t buflen)
{

    LONG err=-1;
    struct PsdPipe *pp;
    struct MsgPort *mp;

    if((mp = CreateMsgPort()))
    {
        if((pp = psdAllocPipe(ncp->ncp_Device, mp, NULL)))
        {

            psdSetAttrs(PGA_PIPE, pp,
                PPA_NakTimeout, FALSE,
                PPA_NakTimeoutTime, 5000,
                PPA_AllowRuntPackets, TRUE,
                TAG_END);

           // bug("urndis_ctrl_msg:pipesetup( %ld,%ld,%ld,%ld,%ld )\n",pp, rt, r, value, index);
            psdPipeSetup(pp, rt, r, value, index);
           // bug("urndis_ctrl_msg:psdDoPipe(%ld,%ld,%ld)\n",pp, buf, buflen);
            err = psdDoPipe(pp, buf, buflen);
            if(err!=0){
                KPRINTF(10, ("urndis_ctrl_msg:error %ld,%s\n",err,psdNumToStr(NTS_IOERR, err, "unknown")));
            }
            psdFreePipe(pp);
        }
        DeleteMsgPort(mp);
    }

    return err;
}


uint32_t urndis_ctrl_send(struct NepClassEth *ncp, void *buf, size_t len)
{
    LONG err;
    LONG sc_ifaceno_ctl=0;

    KPRINTF(10, ("urndis_ctrl_send:\n"));
    //dumpmem(buf,len);

    err = urndis_ctrl_msg(ncp, UT_WRITE_CLASS_INTERFACE, UR_GET_STATUS, sc_ifaceno_ctl, 0, buf, len);

    if (err != 0){
        KPRINTF(10, ("%s:urndis_ctrl_send error %ld\n",DEVNAME,err));
    }

    return err;
}


#define RNDIS_RESPONSE_LEN 0x400

struct urndis_comp_hdr *
urndis_ctrl_recv(struct NepClassEth *ncp)
{

    struct urndis_comp_hdr  *hdr;
    char            *buf;
    LONG         err;
    LONG sc_ifaceno_ctl=0;


    buf = psdAllocVec(RNDIS_RESPONSE_LEN);
    if (buf == NULL) {
        KPRINTF(10, ("%s: out of memory\n", DEVNAME));
        return NULL;
    }

    err = urndis_ctrl_msg(ncp, UT_READ_CLASS_INTERFACE, UR_CLEAR_FEATURE,
        sc_ifaceno_ctl, 0, buf, RNDIS_RESPONSE_LEN);

    if (err != 0) {
        KPRINTF(10, ("%s: urndis_comp_hdr error\n", DEVNAME));
        psdFreeVec(buf);
        return NULL;
    }

    hdr = (struct urndis_comp_hdr *)buf;
    KPRINTF(10, ("%s: urndis_ctrl_recv: type 0x%lx len %lu\n",
        DEVNAME,
        hdr->rm_type,
        letoh32(hdr->rm_len)));

    //dumpmem(buf,hdr->rm_len);

    if (letoh32(hdr->rm_len) > RNDIS_RESPONSE_LEN) {
        KPRINTF(10, ("%s: ctrl message error: wrong size %lu > %lu\n",
            DEVNAME,
            letoh32(hdr->rm_len),
            RNDIS_RESPONSE_LEN));
        psdFreeVec(buf);
        return NULL;
    }

    return hdr;
}


uint32_t
urndis_ctrl_handle_init(struct NepClassEth *ncp,
    const struct urndis_comp_hdr *hdr)
{
    const struct urndis_init_comp   *msg;

    msg = (struct urndis_init_comp *) hdr;

    KPRINTF(10, ("%s: urndis_ctrl_handle_init: len %lu rid %lu status 0x%lx "
        "ver_major %lu ver_minor %lu devflags 0x%lx medium 0x%lx pktmaxcnt %lu "
        "pktmaxsz %lu align %lu aflistoffset %lu aflistsz %lu\n",
        DEVNAME,
        letoh32(msg->rm_len),
        letoh32(msg->rm_rid),
        letoh32(msg->rm_status),
        letoh32(msg->rm_ver_major),
        letoh32(msg->rm_ver_minor),
        letoh32(msg->rm_devflags),
        letoh32(msg->rm_medium),
        letoh32(msg->rm_pktmaxcnt),
        letoh32(msg->rm_pktmaxsz),
        letoh32(msg->rm_align),
        letoh32(msg->rm_aflistoffset),
        letoh32(msg->rm_aflistsz)));

    if (letoh32(msg->rm_status) != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: init failed 0x%lx\n",
            DEVNAME,
            letoh32(msg->rm_status)));

        return letoh32(msg->rm_status);
    }

    if (letoh32(msg->rm_devflags) != RNDIS_DF_CONNECTIONLESS) {
        KPRINTF(10, ("%s: wrong device type (current type: 0x%lx)\n",
            DEVNAME,
            letoh32(msg->rm_devflags)));

        return RNDIS_STATUS_FAILURE;
    }

    if (letoh32(msg->rm_medium) != RNDIS_MEDIUM_802_3) {
        KPRINTF(10, ("%s: medium not 802.3 (current medium: 0x%lx)\n",
            DEVNAME, letoh32(msg->rm_medium)));

        return RNDIS_STATUS_FAILURE;
    }

    ncp->sc_lim_pktsz = letoh32(msg->rm_pktmaxsz);

    return letoh32(msg->rm_status);
}


uint32_t
urndis_ctrl_handle_query(struct NepClassEth *ncp,
    const struct urndis_comp_hdr *hdr, void **buf, size_t *bufsz)
{
    const struct urndis_query_comp  *msg;

    msg = (struct urndis_query_comp *) hdr;

    KPRINTF(10, ("%s: urndis_ctrl_handle_query: len %lu rid %lu status 0x%lx "
        "buflen %lu bufoff %lu\n",
        DEVNAME,
        letoh32(msg->rm_len),
        letoh32(msg->rm_rid),
        letoh32(msg->rm_status),
        letoh32(msg->rm_infobuflen),
        letoh32(msg->rm_infobufoffset)));

    if (buf && bufsz) {
        *buf = NULL;
        *bufsz = 0;
    }

    if (letoh32(msg->rm_status) != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: query failed 0x%lx\n",
            DEVNAME,
            letoh32(msg->rm_status)));

        return letoh32(msg->rm_status);
    }

    if (letoh32(msg->rm_infobuflen) > letoh32(msg->rm_len) ||
        letoh32(msg->rm_infobufoffset) > letoh32(msg->rm_len) - letoh32(msg->rm_infobuflen) ||
        RNDIS_HEADER_OFFSET > letoh32(msg->rm_len) - letoh32(msg->rm_infobuflen) - letoh32(msg->rm_infobufoffset)) {
        KPRINTF(10, ("%s: ctrl message error: invalid query info "
            "len/offset/end_position(%ld/%ld/%ld) -> "
            "go out of buffer limit %ld\n",
            DEVNAME,
            letoh32(msg->rm_infobuflen),
            letoh32(msg->rm_infobufoffset),
            letoh32(msg->rm_infobuflen) +
            letoh32(msg->rm_infobufoffset) + RNDIS_HEADER_OFFSET,
            letoh32(msg->rm_len)));
        return RNDIS_STATUS_FAILURE;
    }

    if (buf && bufsz) {
        *buf = psdAllocVec(letoh32(msg->rm_infobuflen));
        if (*buf == NULL) {
            KPRINTF(10, ("%s: out of memory\n", DEVNAME));
            return RNDIS_STATUS_FAILURE;
        } else {
            char *p;
            *bufsz = letoh32(msg->rm_infobuflen);

            p = (char *)&msg->rm_rid;
            p += letoh32(msg->rm_infobufoffset);
            memcpy(*buf, p, letoh32(msg->rm_infobuflen));
        }
    }

    return letoh32(msg->rm_status);
}


uint32_t
urndis_ctrl_set(struct NepClassEth *ncp, uint32_t oid, void *buf, size_t len)
{
    struct urndis_set_req   *msg;
    uint32_t         rval;
    struct urndis_comp_hdr  *hdr;

    msg = psdAllocVec(sizeof(*msg) + len);
    if (msg == NULL) {
        KPRINTF(10, ("%s: out of memory\n", DEVNAME));
        return RNDIS_STATUS_FAILURE;
    }

    msg->rm_type = htole32(REMOTE_NDIS_SET_MSG);
    msg->rm_len = htole32(sizeof(*msg) + len);
    msg->rm_rid = 0; /* XXX */
    msg->rm_oid = htole32(oid);
    msg->rm_infobuflen = htole32(len);
    if (len != 0) {
        msg->rm_infobufoffset = htole32(20);
        memcpy((char*)msg + 20, buf, len);
    } else
        msg->rm_infobufoffset = 0;
    msg->rm_devicevchdl = 0;

    KPRINTF(10, ("%s: urndis_ctrl_set send: type %lu len %lu rid %lu oid 0x%lx "
        "infobuflen %lu infobufoffset %lu devicevchdl %lu\n",
        DEVNAME,
        letoh32(msg->rm_type),
        letoh32(msg->rm_len),
        letoh32(msg->rm_rid),
        letoh32(msg->rm_oid),
        letoh32(msg->rm_infobuflen),
        letoh32(msg->rm_infobufoffset),
        letoh32(msg->rm_devicevchdl)));

    rval = urndis_ctrl_send(ncp, msg, sizeof(*msg));
    psdFreeVec(msg);

    if (rval != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: set failed\n", DEVNAME));
        return rval;
    }

    if ((hdr = urndis_ctrl_recv(ncp)) == NULL) {
        KPRINTF(10, ("%s: unable to get set response\n", DEVNAME));
        return RNDIS_STATUS_FAILURE;
    }
    rval = urndis_ctrl_handle(ncp, hdr, NULL, NULL);
    if (rval != RNDIS_STATUS_SUCCESS)
        KPRINTF(10, ("%s: set failed 0x%lx\n", DEVNAME, rval));

    return rval;
}


uint32_t
urndis_ctrl_handle_reset(struct NepClassEth *ncp,
    const struct urndis_comp_hdr *hdr)
{
    const struct urndis_reset_comp  *msg;
    uint32_t             rval;

    msg = (struct urndis_reset_comp *) hdr;

    rval = letoh32(msg->rm_status);

    KPRINTF(10, ("%s: urndis_ctrl_handle_reset: len %lu status 0x%lx "
        "adrreset %lu\n",
        DEVNAME,
        letoh32(msg->rm_len),
        rval,
        letoh32(msg->rm_adrreset)));

    if (rval != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: reset failed 0x%lx\n", DEVNAME, rval));
        return rval;
    }

    if (letoh32(msg->rm_adrreset) != 0) {
        uint32_t filter;

        filter = htole32(ncp->sc_filter);
        rval = urndis_ctrl_set(ncp, OID_GEN_CURRENT_PACKET_FILTER,
            &filter, sizeof(filter));
        if (rval != RNDIS_STATUS_SUCCESS) {
            KPRINTF(10, ("%s: unable to reset data filters\n",
                DEVNAME));
            return rval;
        }
    }

    return rval;
}


uint32_t urndis_ctrl_handle(struct NepClassEth *ncp, struct urndis_comp_hdr *hdr,void **buf, size_t *bufsz)
{
    uint32_t rval;

    KPRINTF(10, ("%s: urndis_ctrl_handle\n", DEVNAME));

    if (buf && bufsz) {
        *buf = NULL;
        *bufsz = 0;
    }

    switch (letoh32(hdr->rm_type)) {
        case REMOTE_NDIS_INITIALIZE_CMPLT:
            rval = urndis_ctrl_handle_init(ncp, hdr);
            break;

        case REMOTE_NDIS_QUERY_CMPLT:
            rval = urndis_ctrl_handle_query(ncp, hdr, buf, bufsz);
            break;

        case REMOTE_NDIS_RESET_CMPLT:
            rval = urndis_ctrl_handle_reset(ncp, hdr);
            break;

        case REMOTE_NDIS_KEEPALIVE_CMPLT:
        case REMOTE_NDIS_SET_CMPLT:
            rval = letoh32(hdr->rm_status);
            break;

        default:
            KPRINTF(10, ("%s: ctrl message error: unknown event 0x%lx\n",
                DEVNAME, letoh32(hdr->rm_type)));
            rval = RNDIS_STATUS_FAILURE;
    }

     psdFreeVec(hdr);

    return rval;
}


uint32_t
urndis_ctrl_query(struct NepClassEth *ncp, uint32_t oid,
    void *qbuf, size_t qlen,
    void **rbuf, size_t *rbufsz)
{
    struct urndis_query_req *msg;
    uint32_t         rval;
    struct urndis_comp_hdr  *hdr;

    msg = psdAllocVec(sizeof(*msg) + qlen);
    if (msg == NULL) {
        KPRINTF(10, ("%s: out of memory\n", DEVNAME));
        return RNDIS_STATUS_FAILURE;
    }

    msg->rm_type = htole32(REMOTE_NDIS_QUERY_MSG);
    msg->rm_len = htole32(sizeof(*msg) + qlen);
    msg->rm_rid = 0; /* XXX */
    msg->rm_oid = htole32(oid);
    msg->rm_infobuflen = htole32(qlen);
    if (qlen != 0) {
        msg->rm_infobufoffset = htole32(20);
        memcpy((char*)msg + 20, qbuf, qlen);
    } else
        msg->rm_infobufoffset = 0;
    msg->rm_devicevchdl = 0;

    KPRINTF(10, ("%s: urndis_ctrl_query send: type %lu len %lu rid %lu oid 0x%lx "
        "infobuflen %lu infobufoffset %lu devicevchdl %lu\n",
        DEVNAME,
        letoh32(msg->rm_type),
        letoh32(msg->rm_len),
        letoh32(msg->rm_rid),
        letoh32(msg->rm_oid),
        letoh32(msg->rm_infobuflen),
        letoh32(msg->rm_infobufoffset),
        letoh32(msg->rm_devicevchdl)));

    rval = urndis_ctrl_send(ncp, msg, sizeof(*msg));
    psdFreeVec(msg);

    if (rval != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: query failed\n", DEVNAME));
        return rval;
    }

    if ((hdr = urndis_ctrl_recv(ncp)) == NULL) {
        KPRINTF(10, ("%s: unable to get query response\n", DEVNAME));
        return RNDIS_STATUS_FAILURE;
    }
    rval = urndis_ctrl_handle(ncp, hdr, rbuf, rbufsz);

    return rval;
}


uint32_t urndis_ctrl_init(struct NepClassEth *ncp)
{
    struct urndis_init_req  *msg;
    uint32_t         rval;
    struct urndis_comp_hdr  *hdr;

    msg =  psdAllocVec(sizeof(*msg));
    if (msg == NULL) {
        KPRINTF(10, ("out of memory\n"));
        return RNDIS_STATUS_FAILURE;
    }

    msg->rm_type = htole32(REMOTE_NDIS_INITIALIZE_MSG);
    msg->rm_len = htole32(sizeof(*msg));
    msg->rm_rid = htole32(0);
    msg->rm_ver_major = htole32(1);
    msg->rm_ver_minor = htole32(1);
    msg->rm_max_xfersz = htole32(RNDIS_BUFSZ);

    KPRINTF(10, ("%s: urndis_ctrl_init send: type %lu len %lu rid %lu ver_major %lu "
        "ver_minor %lu max_xfersz %lu\n",
        DEVNAME,
        letoh32(msg->rm_type),
        letoh32(msg->rm_len),
        letoh32(msg->rm_rid),
        letoh32(msg->rm_ver_major),
        letoh32(msg->rm_ver_minor),
        letoh32(msg->rm_max_xfersz)));

    //Delay(50);

    rval = urndis_ctrl_send(ncp, msg, sizeof(*msg));
    psdFreeVec(msg);

    if (rval != 0 ) {
        KPRINTF(10, ("%s: init failed\n", DEVNAME));
        return rval;
    }

    if ((hdr = urndis_ctrl_recv(ncp)) == NULL) {
        KPRINTF(10, ("%s: unable to get init response\n", DEVNAME));
        return RNDIS_STATUS_FAILURE;
    }
    rval = urndis_ctrl_handle(ncp, hdr, NULL, NULL);

    return rval;
}


long urndis_encap(struct NepClassEth *ncp, BYTE *m,LONG len )
{

    struct urndis_packet_msg        *msg;
    msg = (struct urndis_packet_msg *)m;

    memset(msg, 0, sizeof(*msg));
    msg->rm_type = htole32(REMOTE_NDIS_PACKET_MSG);
    msg->rm_len = htole32(sizeof(*msg) + len);

    msg->rm_dataoffset = htole32(RNDIS_DATA_OFFSET);
    msg->rm_datalen = htole32(len);

    //m_copydata(m, 0, len,((char*)msg + RNDIS_DATA_OFFSET + RNDIS_HEADER_OFFSET));

    DB(KPRINTF(10, ("%s: urndis_encap type 0x%lx len %lu data(off %lu len %lu)\n",
        DEVNAME,
        letoh32(msg->rm_type),
        letoh32(msg->rm_len),
        letoh32(msg->rm_dataoffset),
        letoh32(msg->rm_datalen))));

    return(sizeof(*msg));
}


void urndis_decap(struct NepClassEth *ncp, const UBYTE *buf, const LONG datalen)
{

    struct urndis_packet_msg    *msg;
    int          offset;
    int         len;

    offset = 0;
    len = datalen;

    while (len > 0) {
        msg = (struct urndis_packet_msg *)(buf + offset);
        DB(KPRINTF(10, ("%s: urndis_decap buffer size left %lu\n", DEVNAME,len)));

        if (len < sizeof(*msg)) {
            KPRINTF(10, ("%s: urndis_decap invalid buffer len %lu < "
                "minimum header %lu\n",
                DEVNAME,
                len,
                sizeof(*msg)));
            return;
        }

        DB(KPRINTF(10, ("%s: urndis_decap len %lu data(off:%lu len:%lu) "
            "oobdata(off:%lu len:%lu nb:%lu) perpacket(off:%lu len:%lu)\n",
            DEVNAME,
            letoh32(msg->rm_len),
            letoh32(msg->rm_dataoffset),
            letoh32(msg->rm_datalen),
            letoh32(msg->rm_oobdataoffset),
            letoh32(msg->rm_oobdatalen),
            letoh32(msg->rm_oobdataelements),
            letoh32(msg->rm_pktinfooffset),
            letoh32(msg->rm_pktinfooffset))));

        if (letoh32(msg->rm_type) != REMOTE_NDIS_PACKET_MSG) {
            KPRINTF(10, ("%s: urndis_decap invalid type 0x%lx != 0x%lx\n",
                DEVNAME,
                letoh32(msg->rm_type),
                REMOTE_NDIS_PACKET_MSG));
            return;
        }
        if (letoh32(msg->rm_len) < sizeof(*msg)) {
            KPRINTF(10, ("%s: urndis_decap invalid msg len %lu < %lu\n",
                DEVNAME,
                letoh32(msg->rm_len),
                sizeof(*msg)));
            return;
        }
        if (letoh32(msg->rm_len) > len) {
            KPRINTF(10, ("%s: urndis_decap invalid msg len %lu > buffer "
                "len %lu\n",
                DEVNAME,
                letoh32(msg->rm_len),
                len));
            return;
        }

        if (letoh32(msg->rm_dataoffset) +
            letoh32(msg->rm_datalen) + RNDIS_HEADER_OFFSET
                > letoh32(msg->rm_len)) {
            KPRINTF(10, ("%s: urndis_decap invalid data "
                "len/offset/end_position(%lu/%lu/%lu) -> "
                "go out of receive buffer limit %lu\n",
                DEVNAME,
                letoh32(msg->rm_datalen),
                letoh32(msg->rm_dataoffset),
                letoh32(msg->rm_dataoffset) +
                letoh32(msg->rm_datalen) + RNDIS_HEADER_OFFSET,
                letoh32(msg->rm_len)));
            return;
        }

        if (letoh32(msg->rm_datalen) < sizeof(struct EtherPacketHeader)) {
            KPRINTF(10, ("%s: urndis_decap invalid ethernet size "
                "%ld < %ld\n",
                DEVNAME,
                letoh32(msg->rm_datalen),
                sizeof(struct EtherPacketHeader)));
            return;
        }

        DB(KPRINTF(10, ("%s: urndis_decap ethernet packet OK,size %ld,offset %ld\n",DEVNAME, letoh32(msg->rm_datalen),offset)));

        nReadPacket(ncp, ((char*)&msg->rm_dataoffset + letoh32(msg->rm_dataoffset)), letoh32(msg->rm_datalen) );

        offset += letoh32(msg->rm_len);
        len -= letoh32(msg->rm_len);
    }
}


void
urndis_attach(struct NepClassEth *ncp)
{

    uint8_t              eaddr[ETHER_ADDR_SIZE];
    void                *buf;
    size_t               bufsz;
    uint32_t             filter;

    urndis_ctrl_init(ncp);

    if (urndis_ctrl_query(ncp, OID_802_3_PERMANENT_ADDRESS, NULL, 0,
        &buf, &bufsz) != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: unable to get hardware address\n", DEVNAME));
        return;
    }
    if (bufsz == ETHER_ADDR_SIZE) {
        memcpy(eaddr, buf, ETHER_ADDR_SIZE);
        KPRINTF(10, ("%s: address %lx:%lx:%lx:%lx:%lx:%lx\n", DEVNAME, eaddr[0],eaddr[1],eaddr[2],eaddr[3],eaddr[4],eaddr[5]));
        psdFreeVec(buf);
    } else {
        KPRINTF(10, ("%s: invalid address\n", DEVNAME));
        psdFreeVec(buf);
        return;
    }

    /* Initialize packet filter */
    ncp->sc_filter = RNDIS_PACKET_TYPE_BROADCAST;
    ncp->sc_filter |= RNDIS_PACKET_TYPE_ALL_MULTICAST;
    filter = htole32(ncp->sc_filter);
    if (urndis_ctrl_set(ncp, OID_GEN_CURRENT_PACKET_FILTER, &filter,
        sizeof(filter)) != RNDIS_STATUS_SUCCESS) {
        KPRINTF(10, ("%s: unable to set data filters\n", DEVNAME));
        return;
    }

    KPRINTF(10, ("%s: urndis_attach OK!\n", DEVNAME));

    CopyMem(eaddr, ncp->ncp_MacAddress, ETHER_ADDR_SIZE);
    CopyMem(eaddr, ncp->ncp_ROMAddress, ETHER_ADDR_SIZE);

}

