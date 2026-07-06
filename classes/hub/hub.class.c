/*
 *----------------------------------------------------------------------------
 *                         hub class for poseidon
 *----------------------------------------------------------------------------
 *                   By Chris Hodges <chrisly@platon42.de>
 */

#include "debug.h"

#include "hub.class.h"

/* Cross-class dispatch (evicting the USB2 twin of an SS device via the peer
 * hub's class): only the inline LVO macros — the clib prototype (2 args) would
 * clash with our own 3-argument usbDoMethodA export below. USBCLASS_BASE_NAME
 * defaults to UsbClsBase, a local in nNotifyPeerTwinEvict(). */
#include <inline/usbclass.h>

/* /// "Lib Stuff" */
static const STRPTR libname = CLASS_NAME;
static const STRPTR hubunknown = "unknown hub";
static const STRPTR devunknown = "unknown device";
        
int libInit(struct NepHubBase * nh)
{
    KPRINTF(10, ("libInit nh: 0x%08lx EXEC_BASE_NAME: 0x%08lx\n", nh, EXEC_BASE_NAME));

    nh->nh_UtilityBase = OpenLibrary("utility.library", 39);

#define UtilityBase nh->nh_UtilityBase

    if(UtilityBase)
    {
        NewList(&nh->nh_Bindings);
        InitSemaphore(&nh->nh_Adr0Sema);
    } else {
        KPRINTF(20, ("libInit: OpenLibrary(\"utility.library\", 39) failed!\n"));
        return FALSE;
    }

    KPRINTF(10, ("libInit: Ok\n"));
    return TRUE;
}

int libExpunge(struct NepHubBase * nh)
{
    KPRINTF(10, ("libExpunge nh: 0x%08lx EXEC_BASE_NAME: 0x%08lx\n", nh, EXEC_BASE_NAME));
    CloseLibrary(UtilityBase);
    nh->nh_UtilityBase = NULL;
    return TRUE;
}

/* \\\ */

/*
 * ***********************************************************************
 * * Library functions                                                   *
 * ***********************************************************************
 */

/* /// "usbAttemptDeviceBinding()" */
struct NepClassHub * usbAttemptDeviceBinding(struct NepHubBase *nh, struct PsdDevice *pd)
{
    struct Library *ps;
    IPTR devclass;
    KPRINTF(1, ("nepHubAttemptDeviceBinding(0x%08lx)\n", pd));

    if((ps = OpenLibrary("poseidon.library", 4)))
    {
        psdGetAttrs(PGA_DEVICE, pd,
                    DA_Class, &devclass,
                    TAG_DONE);
        CloseLibrary(ps);
        if(devclass == HUB_CLASSCODE)
        {
            return(usbForceDeviceBinding(nh, pd));
        }
    }
    return(NULL);
}
/* \\\ */

/* /// "usbForceDeviceBinding()" */
struct NepClassHub * usbForceDeviceBinding(struct NepHubBase * nh, struct PsdDevice *pd)
{
    struct Library *ps;
    struct NepClassHub *nch;
    STRPTR devname;
    char buf[64];
    struct Task *tmptask;

    KPRINTF(1, ("nepHubAttemptDeviceBinding(0x%08lx)\n", pd));

    if((ps = OpenLibrary("poseidon.library", 4)))
    {
        psdGetAttrs(PGA_DEVICE, pd,
                    DA_ProductName, &devname,
                    TAG_DONE);
        if (!devname) devname = hubunknown;
        if((nch = psdAllocVec(sizeof(struct NepClassHub))))
        {
            nch->nch_HubBase = nh;
            nch->nch_Device = pd;
            psdSafeRawDoFmt(buf, 64, "hub.class<0x%08lx>", nch);
            nch->nch_ReadySignal = SIGB_SINGLE;
            nch->nch_ReadySigTask = FindTask(NULL);
            SetSignal(0, SIGF_SINGLE);
            if((tmptask = psdSpawnSubTask(buf, nHubTask, nch)))
            {
                psdBorrowLocksWait(tmptask, 1UL<<nch->nch_ReadySignal);
                if(nch->nch_Task)
                {
                    nch->nch_ReadySigTask = NULL;
                    //FreeSignal(nch->nch_ReadySignal);
                    psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                   "I'm in love with hub '%s'.",
                                   devname);

                    Forbid();
                    AddTail(&nh->nh_Bindings, &nch->nch_Node);
                    Permit();
                    CloseLibrary(ps);
                    return(nch);
                }
            }
            nch->nch_ReadySigTask = NULL;
            //FreeSignal(nch->nch_ReadySignal);
            psdFreeVec(nch);
        }
        CloseLibrary(ps);
    }
    return(NULL);
}
/* \\\ */

/* /// "usbReleaseDeviceBinding()" */
void usbReleaseDeviceBinding(struct NepHubBase *nh, struct NepClassHub *nch)
{
    struct Library *ps;
    STRPTR devname;

    KPRINTF(1, ("nepHubReleaseDeviceBinding(0x%08lx)\n", nch));
    if((ps = OpenLibrary("poseidon.library", 4)))
    {
        Forbid();
        nch->nch_ReadySignal = SIGB_SINGLE;
        nch->nch_ReadySigTask = FindTask(NULL);
        if(nch->nch_Task)
        {
            KPRINTF(1, ("Sending Break\n"));
            Signal(nch->nch_Task, SIGBREAKF_CTRL_C);
        }
        Permit();
        while(nch->nch_Task)
        {
            psdBorrowLocksWait(nch->nch_Task, 1UL<<nch->nch_ReadySignal);
        }
        KPRINTF(1, ("Task gone\n"));
        //FreeSignal(nch->nch_ReadySignal);
        psdGetAttrs(PGA_DEVICE, nch->nch_Device, DA_ProductName, &devname, TAG_END);
        if (!devname) devname = hubunknown;
        psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                       "Time to get rid of '%s'!",
                       devname);
        Forbid();
        Remove(&nch->nch_Node);
        Permit();

        psdFreeVec(nch);
        CloseLibrary(ps);
    }
}
/* \\\ */

/* /// "usbGetAttrsA()" */
LONG (usbGetAttrsA)(ULONG type asm("d0"), APTR usbstruct asm("a0"), struct TagItem * tags asm("a1"), struct NepHubBase * nh asm("a6"))
{

    struct TagItem *ti;
    LONG count = 0;

    KPRINTF(1, ("nepHubGetAttrsA(%ld, 0x%08lx, 0x%08lx)\n", type, usbstruct, tags));
    switch(type)
    {
        case UGA_CLASS:
             if((ti = FindTagItem(UCCA_Priority, tags)))
             {
                 *((SIPTR *) ti->ti_Data) = 0;
                 count++;
             }
             if((ti = FindTagItem(UCCA_Description, tags)))
             {
                 *((STRPTR *) ti->ti_Data) = "Root/external hub base class";
                 count++;
             }
             if((ti = FindTagItem(UCCA_HasClassCfgGUI, tags)))
             {
                 *((IPTR *) ti->ti_Data) = FALSE;
                 count++;
             }
             if((ti = FindTagItem(UCCA_HasBindingCfgGUI, tags)))
             {
                 *((IPTR *) ti->ti_Data) = FALSE;
                 count++;
             }
             if((ti = FindTagItem(UCCA_AfterDOSRestart, tags)))
             {
                 *((IPTR *) ti->ti_Data) = FALSE;
                 count++;
             }
             if((ti = FindTagItem(UCCA_UsingDefaultCfg, tags)))
             {
                 *((IPTR *) ti->ti_Data) = TRUE;
                 count++;
             }
             if((ti = FindTagItem(UCCA_SupportsSuspend, tags)))
             {
                 *((IPTR *) ti->ti_Data) = TRUE;
                 count++;
             }
             break;

         case UGA_BINDING:
             if((ti = FindTagItem(UCBA_UsingDefaultCfg, tags)))
             {
                 *((IPTR *) ti->ti_Data) = TRUE;
                 count++;
             }
             break;
    }
    return(count);
}
/* \\\ */

/* /// "usbSetAttrsA()" */
LONG (usbSetAttrsA)(ULONG type asm("d0"), APTR usbstruct asm("a0"), struct TagItem * tags asm("a1"), struct NepHubBase * nh asm("a6"))
{
    return(0);
}
/* \\\ */

/* /// "usbDoMethodA()" */
IPTR (usbDoMethodA)(ULONG methodid asm("d0"), IPTR * methoddata asm("a1"), struct NepHubBase * nh asm("a6"))
{

    struct NepClassHub *nch;

    KPRINTF(1, ("Do Method %ld\n", methodid));
    switch(methodid)
    {
        case UCM_AttemptDeviceBinding:
            return((IPTR) usbAttemptDeviceBinding(nh, (struct PsdDevice *) methoddata[0]));

        case UCM_ForceDeviceBinding:
            return((IPTR) usbForceDeviceBinding(nh, (struct PsdDevice *) methoddata[0]));

        case UCM_ReleaseDeviceBinding:
            usbReleaseDeviceBinding(nh, (struct NepClassHub *) methoddata[0]);
            return(TRUE);

        case UCM_HubPowerCyclePort:
        case UCM_HubDisablePort:
        {
            struct PsdDevice *pd = (struct PsdDevice *) methoddata[0];
            ULONG port = (ULONG) methoddata[1];
            if(!(pd && port))
            {
                KPRINTF(20, ("HubPowerCycle/DisablePort Params Null!\n"));
                return(FALSE);
            }
            Forbid();
            nch = (struct NepClassHub *) nh->nh_Bindings.lh_Head;
            while(nch->nch_Node.ln_Succ)
            {
                if(nch->nch_Device == pd)
                {
                    KPRINTF(20, ("HubPowerCycle/DisablePort Dev found (port %ld)!\n", port));
                    if(port <= nch->nch_NumPorts)
                    {
                        nch->nch_DisablePort |= 1UL<<port;
                        if(methodid == UCM_HubPowerCyclePort)
                        {
                            nch->nch_PowerCycle |= 1UL<<port;
                        }
                        if(nch->nch_Task)
                        {
                            Signal(nch->nch_Task, (1L<<nch->nch_TaskMsgPort->mp_SigBit));
                        }
                        Permit();
                        return(TRUE);
                    }
                    break;
                }
                nch = (struct NepClassHub *) nch->nch_Node.ln_Succ;
            }
            Permit();
            return(FALSE);
        }

        case UCM_HubClassScan:
        {
            nch = (struct NepClassHub *) methoddata[0];
            Forbid();
            nch->nch_ClassScan = TRUE;
            if(nch->nch_Task)
            {
                Signal(nch->nch_Task, (1L<<nch->nch_TaskMsgPort->mp_SigBit));
            }
            Permit();
            return(TRUE);
        }

        case UCM_AttemptSuspendDevice:
        case UCM_AttemptResumeDevice:
        case UCM_HubClaimAppBinding:
        case UCM_HubReleaseIfBinding:
        case UCM_HubReleaseDevBinding:
        case UCM_HubSuspendDevice:
        case UCM_HubResumeDevice:
        {
            struct NepHubMsg nhm;
            struct Library *ps;
            nch = (struct NepClassHub *) methoddata[0];
            nhm.nhm_Result = (IPTR) NULL;
            nhm.nhm_MethodID = methodid;
            nhm.nhm_Params = methoddata;
            if((ps = OpenLibrary("poseidon.library", 4)))
            {
                if(nch->nch_Task == FindTask(NULL))
                {
                    // if we would send the message to ourself, we would deadlock, so handle this directly
                    nHandleHubMethod(nch, &nhm);
                } else {
                    nhm.nhm_Msg.mn_ReplyPort = CreateMsgPort();
                    nhm.nhm_Msg.mn_Length = sizeof(struct NepHubMsg);
                    Forbid();
                    if(nch->nch_Task && nhm.nhm_Msg.mn_ReplyPort)
                    {
                        PutMsg(nch->nch_CtrlMsgPort, &nhm.nhm_Msg);
                        Permit();
                        while(!GetMsg(nhm.nhm_Msg.mn_ReplyPort))
                        {
                            psdBorrowLocksWait(nch->nch_Task, 1UL<<nhm.nhm_Msg.mn_ReplyPort->mp_SigBit);
                        }
                    } else {
                        Permit();
                    }
                    if(nhm.nhm_Msg.mn_ReplyPort)
                    {
                        DeleteMsgPort(nhm.nhm_Msg.mn_ReplyPort);
                    }
                }
                CloseLibrary(ps);
            }
            return(nhm.nhm_Result);
        }

        default:
            break;
    }
    return(0);
}
/* \\\ */

#undef ps
#define ps nch->nch_Base

/* /// "nFindPeerHub()" */
/* Find the other half of the same physical USB3 hub: another bound hub device
 * carrying the identical BOS Container ID but the opposite half role (the spec
 * requires both halves to report the same Container ID, and identical port
 * numbering). VID/PID are deliberately not compared — the halves legitimately
 * differ (e.g. VIA VL817: 2109:2817 vs 2109:0817).
 * Caller must hold psdLockReadPBase(). */
struct PsdDevice * nFindPeerHub(struct NepClassHub *nch)
{
    if(!nch->nch_ContainerId)
    {
        return(NULL);
    }
    struct PsdDevice *pd = NULL;
    while((pd = psdGetNextDevice(pd)))
    {
        if(pd == nch->nch_Device)
        {
            continue;
        }
        IPTR devclass = 0;
        IPTR isconnected = 0;
        IPTR isdead = 0;
        IPTR proto = 0;
        IPTR issuperspeed = 0;
        APTR binding = NULL;
        UBYTE *containerid = NULL;
        psdGetAttrs(PGA_DEVICE, pd,
                    DA_Class, &devclass,
                    DA_IsConnected, &isconnected,
                    DA_IsDead, &isdead,
                    DA_Binding, &binding,
                    DA_Protocol, &proto,
                    DA_IsSuperspeed, &issuperspeed,
                    DA_ContainerId, &containerid,
                    TAG_END);
        if((devclass != HUB_CLASSCODE) || (!isconnected) || isdead || (!binding) || (!containerid))
        {
            continue;
        }
        BOOL peerisss = (proto == 3) || issuperspeed;
        if((peerisss == nch->nch_IsSSHalf) || memcmp(containerid, nch->nch_ContainerId, 16))
        {
            continue;
        }
        return(pd);
    }
    return(NULL);
}
/* \\\ */

/* /// "nNotifyPeerTwinEvict()" */
/* SS half only: after enumerating a child on a port, evict any USB2 twin the
 * companion 2.0 half may have captured on the same physical connector. Sent
 * unconditionally (not only when a twin is visible) — the twin may still be
 * mid-enumeration; the persistent nch_DisablePort bit is what closes that
 * race. Deliberately does not touch PORT_POWER: Vbus is one shared rail per
 * connector, unpowering via the 2.0 half could brown-out the SS link. */
void nNotifyPeerTwinEvict(struct NepClassHub *nch, UWORD port)
{
    if((!nch->nch_IsSSHalf) || (!nch->nch_ContainerId))
    {
        return;
    }
    psdLockReadPBase();
    struct PsdDevice *peerpd = nFindPeerHub(nch);
    if(peerpd)
    {
        APTR binding = NULL;
        APTR puc = NULL;
        struct Library *UsbClsBase = NULL;
        Forbid();
        psdGetAttrs(PGA_DEVICE, peerpd,
                    DA_Binding, &binding,
                    DA_BindingClass, &puc,
                    TAG_END);
        if(binding && puc)
        {
            psdGetAttrs(PGA_USBCLASS, puc, UCA_ClassBase, &UsbClsBase, TAG_END);
        }
        if(UsbClsBase)
        {
            KPRINTF(2, ("Evicting USB2 twin at peer hub 0x%08lx port %ld\n", peerpd, port));
            usbDoMethod(UCM_HubDisablePort, peerpd, (IPTR) port);
        }
        Permit();
    }
    psdUnlockPBase();
}
/* \\\ */

/* /// "nPortShadowedByPeer()" */
/* 2.0 half only: TRUE when the SS half of the same physical hub already has
 * (or is currently enumerating — hub/port attrs are set before reset) a child
 * on the same connector. The USB2 presence is then just the twin of the SS
 * device and must not be enumerated. */
BOOL nPortShadowedByPeer(struct NepClassHub *nch, UWORD port)
{
    if(nch->nch_IsSSHalf || (!nch->nch_ContainerId))
    {
        return(FALSE);
    }
    BOOL shadowed = FALSE;
    psdLockReadPBase();
    struct PsdDevice *peerpd = nFindPeerHub(nch);
    if(peerpd)
    {
        struct PsdDevice *pd = NULL;
        while((pd = psdGetNextDevice(pd)))
        {
            APTR hubpd = NULL;
            IPTR hubport = 0;
            psdGetAttrs(PGA_DEVICE, pd,
                        DA_HubDevice, &hubpd,
                        DA_AtHubPortNumber, &hubport,
                        TAG_END);
            if((hubpd == peerpd) && (hubport == port))
            {
                shadowed = TRUE;
                break;
            }
        }
    }
    psdUnlockPBase();
    return(shadowed);
}
/* \\\ */

/* /// "nConnectShadowDebounce()" */
/* 2.0 half with a known SS peer: give the SS half time to win the connector
 * before acting on a fresh USB2 connect (SS devices present a transient D+
 * pullup until link training succeeds). Returns TRUE when the connect turned
 * out to be the twin of an SS device. */
BOOL nConnectShadowDebounce(struct NepClassHub *nch, UWORD port)
{
    if(nch->nch_IsSSHalf || (!nch->nch_ContainerId))
    {
        return(FALSE);
    }
    if(nPortShadowedByPeer(nch, port))
    {
        return(TRUE);
    }
    psdLockReadPBase();
    BOOL haspeer = (nFindPeerHub(nch) != NULL);
    psdUnlockPBase();
    if(!haspeer)
    {
        return(FALSE);
    }
    psdDelayMS(500);
    return(nPortShadowedByPeer(nch, port));
}
/* \\\ */

/* /// "nHubTask()" */
void nHubTask()
{

    struct NepClassHub *nch;
    struct PsdPipe *pp;
    ULONG sigmask;
    ULONG sigs;
    UWORD num;
    LONG ioerr;
    struct UsbPortStatus uhps;
    struct UsbHubStatus uhhs;
    ULONG count;
    struct PsdDevice *pd;
    STRPTR devname;
    struct NepHubMsg *nhm;

    if((nch = nAllocHub()))
    {
        Forbid();
        if(nch->nch_ReadySigTask)
        {
            Signal(nch->nch_ReadySigTask, 1L<<nch->nch_ReadySignal);
        }
        Permit();
        count = 0;
        for(num = 1; num <= nch->nch_NumPorts; num++)
        {
            if(nPortShadowedByPeer(nch, num))
            {
                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                               "Port %ld is the USB 2.0 twin of a superspeed device, skipping.",
                               num);
                continue;
            }
            if(((nch->nch_Downstream)[num-1] = pd = nConfigurePort(nch, num)))
            {
                psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                if (!devname) devname = devunknown;
                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                               "Detected device '%s' at port %ld. I like it.",
                               devname, num);
                count++;
            }
        }
        if(count)
        {
            psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                           "Hub has added %ld device(s). That'll be fun!",
                           count);
        }
        // do a class scan
        for(num = 1; num <= nch->nch_NumPorts; num++)
        {
            if((pd = (nch->nch_Downstream)[num-1]))
            {
                psdHubClassScan(pd);
            }
        }
        sigmask = (1L<<nch->nch_TaskMsgPort->mp_SigBit)|(1L<<nch->nch_CtrlMsgPort->mp_SigBit)|SIGBREAKF_CTRL_C;
        nch->nch_Running = TRUE;
        nch->nch_IOStarted = FALSE;
        do
        {
            if(nch->nch_Running && (!nch->nch_IOStarted))
            {
                psdSendPipe(nch->nch_EP1Pipe, nch->nch_PortChanges, (nch->nch_NumPorts+8)>>3);
                nch->nch_IOStarted = TRUE;
            }
            if(nch->nch_DisablePort || nch->nch_ClassScan)
            {
                /* Don't sleep on queued port work: the wake-up Signal may have
                   been consumed by a pipe wait inside a concurrent enumeration
                   (e.g. a twin-evict request arriving mid-nConfigurePort). */
                sigs = SetSignal(0, 0) & SIGBREAKF_CTRL_C;
            } else {
                sigs = Wait(sigmask);
            }
            while((nhm = (struct NepHubMsg *) GetMsg(nch->nch_CtrlMsgPort)))
            {
                nHandleHubMethod(nch, nhm);

                ReplyMsg((struct Message *) nhm);
            }
            if(nch->nch_DisablePort)
            {
                for(num = 1; num <= nch->nch_NumPorts; num++)
                {
                    if((nch->nch_DisablePort) & (1L<<num))
                    {
                        nch->nch_DisablePort &= ~(1L<<num);
                        /* Remove device */
                        if((pd = (nch->nch_Downstream)[num-1]))
                        {
                            psdSetAttrs(PGA_DEVICE, pd, DA_IsConnected, FALSE, TAG_END);
                            psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                            if (!devname) devname = devunknown;
                            psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                           "Zapping device '%s' at port %ld!",
                                           devname, num);
                            psdFreeDevice(pd);
                            psdSendEvent(EHMB_REMDEVICE, pd, NULL);
                            (nch->nch_Downstream)[num-1] = NULL;
                            pd = NULL;
                            /* disable port */
                            psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                         USR_CLEAR_FEATURE, UFS_PORT_ENABLE, (ULONG) num);
                            ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                            if(ioerr)
                            {
                                psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                               "CLEAR_PORT_ENABLE failed: %s (%ld)",
                                               psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                                KPRINTF(1, ("CLEAR_PORT_ENABLE failed %ld.\n", ioerr));
                            }
                        }
                        if(nch->nch_PowerCycle & (1<<num))
                        {
                            KPRINTF(2, ("Powercycle request for port %lu\n", num));
                            nch->nch_PowerCycle &= ~(1L<<num);

                            /* Wait for device to settle */
                            psdDelayMS(250);
                            if(nPortShadowedByPeer(nch, num))
                            {
                                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                               "Port %ld is the USB 2.0 twin of a superspeed device, skipping.",
                                               num);
                            }
                            else if(((nch->nch_Downstream)[num-1] = pd = nConfigurePort(nch, num)))
                            {
                                psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                                if (!devname) devname = devunknown;
                                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                               "Device '%s' returned. Happy happy joy joy.",
                                               devname);
                                psdHubClassScan(pd);
                            }
                        }
                    }
                }
            }
            if(nch->nch_ClassScan)
            {
                nch->nch_ClassScan = FALSE;
                for(num = 1; num <= nch->nch_NumPorts; num++)
                {
                    if((pd = (nch->nch_Downstream)[num-1]))
                    {
                        psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                        if (!devname) devname = devunknown;
                        psdHubClassScan(pd);
                    }
                }
            }
            while((pp = (struct PsdPipe *) GetMsg(nch->nch_TaskMsgPort)))
            {
                if(pp == nch->nch_EP1Pipe)
                {
                    nch->nch_IOStarted = FALSE;
                    ioerr = psdGetPipeError(nch->nch_EP1Pipe);
                    if(ioerr == UHIOERR_TIMEOUT)
                    {
                        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                       "Hub involuntarily gone! Disconnecting...");
                        psdSetAttrs(PGA_DEVICE, nch->nch_Device,
                                    DA_IsConnected, FALSE,
                                    TAG_END);
                        nch->nch_PortChanges[0] = 0xff;
                        nch->nch_PortChanges[1] = 0xff;
                        nch->nch_PortChanges[2] = 0xff;
                        nch->nch_PortChanges[3] = 0xff;
                        sigs |= SIGBREAKF_CTRL_C;
                    }
                    if((!ioerr) || (ioerr == UHIOERR_TIMEOUT))
                    {
                        KPRINTF(2, ("Port changed at 0x%08lx, Numports=%ld!\n", nch->nch_PortChanges[0], nch->nch_NumPorts));

                        if(nch->nch_PortChanges[0] & 1)
                        {
                            psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_CLASS|URTF_DEVICE,
                                         USR_GET_STATUS, 0, 0);
                            ioerr = psdDoPipe(nch->nch_EP0Pipe, &uhhs, sizeof(struct UsbHubStatus));
                            uhhs.wHubStatus = AROS_WORD2LE(uhhs.wHubStatus);
                            uhhs.wHubChange = AROS_WORD2LE(uhhs.wHubChange);
                            if(!ioerr)
                            {
                                if(uhhs.wHubStatus & UHSF_OVER_CURRENT)
                                {
                                    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                   "Hub over-current situation detected! Unpowering ALL ports!");
                                    for(num = 1; num <= nch->nch_NumPorts; num++)
                                    {
                                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                                     USR_CLEAR_FEATURE, UFS_PORT_POWER, (ULONG) num);
                                        ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                        if(ioerr)
                                        {
                                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                           "PORT_POWER for port %ld failed: %s (%ld)",
                                                           num, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                                            KPRINTF(1, ("PORT_POWER for port %ld failed %ld!\n", num, ioerr));
                                        }

                                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                                     USR_CLEAR_FEATURE, UFS_C_PORT_OVER_CURRENT, (ULONG) num);
                                        psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                    }
                                    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_DEVICE,
                                                 USR_CLEAR_FEATURE, UFS_C_HUB_OVER_CURRENT, 0);
                                    psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                }
                                if(uhhs.wHubChange & UHSF_LOCAL_POWER_LOST)
                                {
                                    struct PsdConfig *pc = NULL;
                                    struct PsdHardware *phw = NULL;
                                    psdGetAttrs(PGA_DEVICE, nch->nch_Device,
                                                DA_Config, &pc,
                                                DA_Hardware, &phw,
                                                TAG_END);
                                    if(uhhs.wHubStatus & UHSF_LOCAL_POWER_LOST)
                                    {
                                        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                       "Hub is no longer self-powered! Low power conditions may occur.");

                                        if(pc && phw)
                                        {
                                            psdSetAttrs(PGA_CONFIG, pc, CA_SelfPowered, FALSE, TAG_END);
                                            psdCalculatePower(phw);
                                        }
                                    } else {
                                        psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                       "Hub is now self-powered! Yay!");
                                        if(pc && phw)
                                        {
                                            psdSetAttrs(PGA_CONFIG, pc, CA_SelfPowered, TRUE, TAG_END);
                                            psdCalculatePower(phw);
                                        }
                                    }
                                    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_DEVICE,
                                                 USR_CLEAR_FEATURE, UFS_C_HUB_LOCAL_POWER, 0);
                                    psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                }
                            }
                        }

                        for(num = 1; num <= nch->nch_NumPorts; num++)
                        {
                            if(nch->nch_PortChanges[num>>3] & (1L<<(num & 7)))
                            {
                                /* Snooping HCDs correlate this status reply with the next
                                   SET_ADDRESS, so keep it out of other hubs' default-address
                                   windows. */
                                if(!nch->nch_CtxHardware)
                                    ObtainSemaphore(&nch->nch_HubBase->nh_Adr0Sema);
                                psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_CLASS|URTF_OTHER,
                                             USR_GET_STATUS, 0, (ULONG) num);
                                ioerr = psdDoPipe(nch->nch_EP0Pipe, &uhps, sizeof(struct UsbPortStatus));
                                uhps.wPortStatus = AROS_WORD2LE(uhps.wPortStatus);
                                uhps.wPortChange = AROS_WORD2LE(uhps.wPortChange);
                                if(ioerr == UHIOERR_TIMEOUT)
                                {
                                    uhps.wPortStatus = 0;
                                    uhps.wPortChange = 0xffff;
                                    ioerr = 0;
                                } else {
                                    nClearPortStatus(nch, num);
                                }
                                if(!nch->nch_CtxHardware)
                                    ReleaseSemaphore(&nch->nch_HubBase->nh_Adr0Sema);
                                if(!ioerr)
                                {
                                    pd = (nch->nch_Downstream)[num-1];
                                    if(uhps.wPortStatus & UPSF_PORT_OVER_CURRENT)
                                    {
                                        if(pd)
                                        {
                                            psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                                            if (!devname) devname = devunknown;
                                        } else {
                                            devname = "a ghost";
                                        }
                                        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                       "Over-current situation detected with %s at port %ld! Unpowering port!",
                                                       devname, num);
                                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                                     USR_CLEAR_FEATURE, UFS_PORT_POWER, (ULONG) num);
                                        ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                        if(ioerr)
                                        {
                                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                           "PORT_POWER for port %ld failed: %s (%ld)",
                                                           num, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                                            KPRINTF(1, ("PORT_POWER for port %ld failed %ld!\n", num, ioerr));
                                        }

                                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                                     USR_CLEAR_FEATURE, UFS_C_PORT_OVER_CURRENT, (ULONG) num);
                                        psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                    }
                                    if(uhps.wPortChange & UPCF_C_PORT_SUSPEND)
                                    {
                                        if((!(uhps.wPortStatus & UPSF_PORT_SUSPEND)) && pd)
                                        {
                                            IPTR oldsusp = 0;
                                            psdGetAttrs(PGA_DEVICE, pd, DA_IsSuspended, &oldsusp, TAG_END);
                                            psdSetAttrs(PGA_DEVICE, pd, DA_IsSuspended, FALSE, TAG_END);
                                            psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                                            if (!devname) devname = devunknown;
                                            if(oldsusp)
                                            {
                                                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                               "Device '%s' at port %ld resumed from remote!",
                                                               devname, num);
                                                psdSendEvent(EHMB_DEVRESUMED, pd, NULL);
                                                psdResumeBindings(pd);
                                            }
                                        }
                                        else if((uhps.wPortStatus & UPSF_PORT_SUSPEND) && pd)
                                        {
                                            psdSetAttrs(PGA_DEVICE, pd, DA_IsSuspended, FALSE, TAG_END);
                                            psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                                            if (!devname) devname = devunknown;
                                            psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                           "Device '%s' at port %ld suspended!",
                                                           devname, num);
                                        } else {
                                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                           "Bogus suspend/resume change on port %ld.",
                                                           num);
                                        }
                                    }
                                    if(uhps.wPortChange & UPCF_C_PORT_CONNECTION)
                                    {
                                        /* Remove device */
                                        if((!(uhps.wPortStatus & UPSF_PORT_CONNECTION)) && pd)
                                        {
                                            psdSetAttrs(PGA_DEVICE, pd, DA_IsConnected, FALSE, TAG_END);
                                            psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                                            if (!devname) devname = devunknown;
                                            psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                           "Device '%s' at port %ld is gone!",
                                                           devname, num);
                                            psdFreeDevice(pd);
                                            psdSendEvent(EHMB_REMDEVICE, pd, NULL);
                                            (nch->nch_Downstream)[num-1] = NULL;
                                            pd = NULL;
                                        }
                                        /* add new device */
                                        if((uhps.wPortStatus & UPSF_PORT_CONNECTION) && (!pd))
                                        {
                                            /* Wait for device to settle */
                                            psdDelayMS(100);
                                            if(nConnectShadowDebounce(nch, num))
                                            {
                                                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                               "Port %ld is the USB 2.0 twin of a superspeed device, skipping.",
                                                               num);
                                            }
                                            else if(((nch->nch_Downstream)[num-1] = pd = nConfigurePort(nch, num)))
                                            {
                                                psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
                                                if (!devname) devname = devunknown;
                                                psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                               "New device '%s' at port %ld. Very nice.",
                                                               devname, num);
                                                psdClassScan();
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        /* Bail out on time out. */
                        if(nch->nch_PortChanges[0] == 0xff)
                        {
                            break;
                        }
                        psdDelayMS(50);
                    } else {
                        if(ioerr != IOERR_ABORTED)
                        {
                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                           "Something weird happened to the status packet, it failed: %s (%ld)",
                                           psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                            psdDelayMS(200);
                        }
                    }
                    break;
                } else {
                    KPRINTF(20, ("Bogus message received!\n"));
                }
            }
        } while(!(sigs & SIGBREAKF_CTRL_C));
        KPRINTF(20, ("Going down the river!\n"));
        if(nch->nch_IOStarted)
        {
            psdAbortPipe(nch->nch_EP1Pipe);
            psdWaitPipe(nch->nch_EP1Pipe);
        }
        psdAddErrorMsg(RETURN_OK, (STRPTR) libname, "Oh no! I've been shot! Arrggghh...");
        nFreeHub(nch);
    }
}
/* \\\ */

/* /// "nAllocHub()" */
struct NepClassHub * nAllocHub(void)
{
    struct UsbHubDesc *uhd;
    struct Task *thistask;
    struct NepClassHub *nch;
    struct UsbHubStatus uhhs;
    APTR parenthub;
    LONG ioerr;
    ULONG len;
    UWORD num;
    UBYTE buf;
    IPTR ishighspeed = 0;
    IPTR prodid;
    IPTR vendid;
    IPTR proto = 0;
    IPTR issuperspeed = 0;
    UBYTE *containerid = NULL;
    BOOL overcurrent = FALSE;
    IPTR ctxhw = 0;
    IPTR ifproto = 0;

    thistask = FindTask(NULL);
    nch = thistask->tc_UserData;
    do
    {
        if(!(nch->nch_Base = OpenLibrary("poseidon.library", 4)))
        {
            Alert(AG_OpenLib);
            break;
        }

        psdGetAttrs(PGA_DEVICE, nch->nch_Device,
                    DA_Hardware, &nch->nch_Hardware,
                    DA_IsHighspeed, &ishighspeed,
                    DA_ProductID, &prodid,
                    DA_VendorID, &vendid,
                    DA_HubDevice, &parenthub,
                    DA_Protocol, &proto,
                    DA_IsSuperspeed, &issuperspeed,
                    DA_ContainerId, &containerid,
                    TAG_END);

        nch->nch_IsRootHub = (parenthub ? FALSE : TRUE);
        nch->nch_IsUSB20 = ishighspeed;
        /* context HCDs have no software-visible default-address phase, so
           enumeration needs no address-0 serialization there */
        psdGetAttrs(PGA_HARDWARE, nch->nch_Hardware,
                    HA_ContextBackend, &ctxhw,
                    TAG_END);
        nch->nch_CtxHardware = ctxhw ? TRUE : FALSE;
        /* USB3 hub pairing: the SS half of a physical USB3 hub is recognized by
           bDeviceProtocol 3 (also through HCD 3.0->2.0 translators) or the
           superspeed flag. An all-zero Container ID (counterfeit hubs) keeps
           pairing disabled for this hub. */
        nch->nch_IsSSHalf = (proto == 3) || issuperspeed;
        nch->nch_ContainerId = containerid;
        if(containerid)
        {
            UWORD cnt;
            for(cnt = 0; (cnt < 16) && (!containerid[cnt]); cnt++);
            if(cnt == 16)
            {
                nch->nch_ContainerId = NULL;
            }
        }
        // try to select multi TT interface first
        nch->nch_Interface = psdFindInterface(nch->nch_Device, NULL,
                                              IFA_Class, HUB_CLASSCODE,
                                              IFA_Protocol, 2,
                                              IFA_AlternateNum, 0xffffffff,
                                              TAG_END);
        if(!nch->nch_Interface)
        {
            // any will do
            nch->nch_Interface = psdFindInterface(nch->nch_Device, NULL,
                                                  IFA_Class, HUB_CLASSCODE,
                                                  TAG_END);
        }
        if((vendid == 0x05E3) && ishighspeed)
        {
            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname, "Genesys Logic hubs are broken and will cause failures with USB 2.0 devices.");
            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname, "If you encounter problems, try the device without the hub.");
            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname, "If this solves the problem, you need to buy a different USB 2.0 hub.");
        }

        if(!nch->nch_Interface)
        {
            KPRINTF(1, ("Ooops!?! No interfaces defined?\n"));
            break;
        }
        nch->nch_EP1 = psdFindEndpoint(nch->nch_Interface, NULL,
                                       EA_IsIn, TRUE,
                                       EA_TransferType, USEAF_INTERRUPT,
                                       TAG_END);

        if(!nch->nch_EP1)
        {
            psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname, "Ooops!?! No endpoints defined?");
            KPRINTF(1, ("Ooops!?! No Endpoints defined?\n"));
            break;
        }
        if((nch->nch_CtrlMsgPort = CreateMsgPort()))
        {
            if((nch->nch_TaskMsgPort = CreateMsgPort()))
            {
                if((nch->nch_EP0Pipe = psdAllocPipe(nch->nch_Device, nch->nch_TaskMsgPort, NULL)))
                {
                    psdSetAttrs(PGA_PIPE, nch->nch_EP0Pipe,
                                PPA_NakTimeout, TRUE,
                                PPA_NakTimeoutTime, 1000,
                                TAG_END);
                    psdSetAltInterface(nch->nch_EP0Pipe, nch->nch_Interface);
                    if((nch->nch_EP1Pipe = psdAllocPipe(nch->nch_Device, nch->nch_TaskMsgPort, nch->nch_EP1)))
                    {
                        psdSetAttrs(PGA_PIPE, nch->nch_EP1Pipe,
                                    PPA_AllowRuntPackets, TRUE,
                                    TAG_END);
                        psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_CLASS|URTF_DEVICE,
                                     USR_GET_DESCRIPTOR, UDT_HUB<<8, 0);
                        ioerr = psdDoPipe(nch->nch_EP0Pipe, &buf, 1);
                        if((!ioerr) || (ioerr == UHIOERR_OVERFLOW))
                        {
                            len = buf;
                            if((uhd = psdAllocVec(len)))
                            {
                                ioerr = psdDoPipe(nch->nch_EP0Pipe, uhd, len);
                                if(!ioerr)
                                {
                                    nch->nch_NumPorts = uhd->bNbrPorts;
                                    nch->nch_HubAttr = AROS_WORD2LE(uhd->wHubCharacteristics);
                                    nch->nch_PwrGoodTime = uhd->bPwrOn2PwrGood<<1;
                                    nch->nch_HubCurrent = uhd->bHubContrCurrent;
                                    nch->nch_Removable = 0;
                                    /* publish the hub facts; DA_HubNumPorts triggers the
                                       HCD update-hub op on context backends, so set the
                                       whole set in one call. Multi-TT is active iff the
                                       protocol-2 alternate was selected above. */
                                    psdGetAttrs(PGA_INTERFACE, nch->nch_Interface,
                                                IFA_Protocol, &ifproto,
                                                TAG_END);
                                    psdSetAttrs(PGA_DEVICE, nch->nch_Device,
                                                DA_HubThinkTime, (nch->nch_HubAttr & UHCM_THINK_TIME)>>UHCS_THINK_TIME,
                                                DA_IsMultiTT, (ifproto == 2) ? TRUE : FALSE,
                                                DA_HubNumPorts, nch->nch_NumPorts,
                                                TAG_END);

                                    for(num = 0; num < ((nch->nch_NumPorts + 7)>>3); num++)
                                    {
                                        nch->nch_Removable |= ((&uhd->DeviceRemovable)[num])<<(num<<3);
                                    }
                                    KPRINTF(2, ("Hub with %ld ports\n"
                                    	   "  Characteristics 0x%04lx\n"
                                           "  PowerGood after %ld ms\n"
                                           "  Power consumption %ld mA\n",
                                           nch->nch_NumPorts,
                                           nch->nch_HubAttr,
                                           nch->nch_PwrGoodTime, nch->nch_HubCurrent));
                                    psdFreeVec(uhd);

                                    psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_CLASS|URTF_DEVICE,
                                                 USR_GET_STATUS, 0, 0);
                                    ioerr = psdDoPipe(nch->nch_EP0Pipe, &uhhs, sizeof(struct UsbHubStatus));
                                    uhhs.wHubStatus = AROS_WORD2LE(uhhs.wHubStatus);
                                    uhhs.wHubChange = AROS_WORD2LE(uhhs.wHubChange);
                                    if(!ioerr)
                                    {
                                        struct PsdConfig *pc = NULL;
                                        struct PsdHardware *phw = NULL;
                                        if(uhhs.wHubStatus & UHSF_OVER_CURRENT)
                                        {
                                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                           "Hub over-current situation detected! Resolve this first!");
                                            //overcurrent = TRUE;
                                        }

                                        psdGetAttrs(PGA_DEVICE, nch->nch_Device,
                                                    DA_Config, &pc,
                                                    DA_Hardware, &phw,
                                                    TAG_END);
                                        if(uhhs.wHubStatus & UHSF_LOCAL_POWER_LOST)
                                        {
                                            if(pc && phw)
                                            {
                                                psdSetAttrs(PGA_CONFIG, pc, CA_SelfPowered, FALSE, TAG_END);
                                                psdCalculatePower(phw);
                                            }
                                        } else {
                                            if(pc && phw)
                                            {
                                                psdSetAttrs(PGA_CONFIG, pc, CA_SelfPowered, TRUE, TAG_END);
                                                psdCalculatePower(phw);
                                            }
                                        }
                                    }
                                    if(!overcurrent)
                                    {
                                        if((nch->nch_Downstream = psdAllocVec((ULONG) nch->nch_NumPorts*sizeof(APTR))))
                                        {
                                            /*for(num = 1; num <= nch->nch_NumPorts; num++)
                                            {
                                                nClearPortStatus(nch, num);
                                            }
                                            psdDelayMS(20);*/
                                            KPRINTF(2, ("Powering up ports...\n"));
                                            for(num = 1; num <= nch->nch_NumPorts; num++)
                                            {
                                                psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                                             USR_SET_FEATURE, UFS_PORT_POWER, (ULONG) num);
                                                ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                                                if(ioerr)
                                                {
                                                    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                                                   "PORT_POWER for port %ld failed: %s (%ld)",
                                                                   num, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                                                    KPRINTF(1, ("PORT_POWER for port %ld failed %ld!\n", num, ioerr));
                                                }
                                            }
                                            psdDelayMS((ULONG) nch->nch_PwrGoodTime + 15);

                                            psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                                                           "Hub with %ld ports successfully configured.",
                                                           nch->nch_NumPorts);

                                            KPRINTF(10, ("%s ready!\n", thistask->tc_Node.ln_Name));
                                            nch->nch_Task = thistask;
                                            return(nch);
                                        } else {
                                            KPRINTF(1, ("No downstream port array memory!\n"));
                                        }
                                    }
                                } else {
                                    psdFreeVec(uhd);
                                    psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                                                   "GET_HUB_DESCRIPTOR (%ld) failed: %s (%ld)",
                                                   len, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                                    KPRINTF(1, ("GET_HUB_DESCRIPTOR (%ld) failed %ld!\n", len, ioerr));
                                }
                            } else {
                                KPRINTF(1, ("No Hub Descriptor memory!\n"));
                            }
                        } else {
                            psdAddErrorMsg(RETURN_FAIL, (STRPTR) libname,
                                           "GET_HUB_DESCRIPTOR (%ld) failed: %s (%ld)",
                                           1, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                            KPRINTF(1, ("GET_HUB_DESCRIPTOR (1) failed %ld!\n", ioerr));
                        }
                        psdFreePipe(nch->nch_EP1Pipe);
                    }
                    psdFreePipe(nch->nch_EP0Pipe);
                }
                DeleteMsgPort(nch->nch_TaskMsgPort);
            }
            DeleteMsgPort(nch->nch_CtrlMsgPort);
        }
    } while(FALSE);
    CloseLibrary(nch->nch_Base);
    Forbid();
    nch->nch_Task = NULL;
    if(nch->nch_ReadySigTask)
    {
        Signal(nch->nch_ReadySigTask, 1L<<nch->nch_ReadySignal);
    }
    return(NULL);
}
/* \\\ */

/* /// "nFreeHub()" */
void nFreeHub(struct NepClassHub *nch)
{
    UWORD num;
    LONG ioerr;
    struct PsdDevice *pd;
    STRPTR devname;
    IPTR isconnected;
    struct Message *msg;

    KPRINTF(1, ("FreeHub\n"));
    psdGetAttrs(PGA_DEVICE, nch->nch_Device, DA_IsConnected, &isconnected, TAG_END);
    for(num = 1; num <= nch->nch_NumPorts; num++)
    {
        KPRINTF(1, ("Iterating Port %ld\n", num));
        /* Remove downstream device */
        pd = (nch->nch_Downstream)[num-1];
        if(pd)
        {
            if(!isconnected)
            {
                psdSetAttrs(PGA_DEVICE, pd, DA_IsConnected, FALSE, TAG_END);
            }
            psdGetAttrs(PGA_DEVICE, pd, DA_ProductName, &devname, TAG_END);
            if (!devname) devname = devunknown;
            psdAddErrorMsg(RETURN_OK, (STRPTR) libname,
                           "My death killed device '%s' at port %ld!",
                           devname, num);
            KPRINTF(1, ("FreeDevice 0x%08lx\n", pd));
            psdFreeDevice(pd);
            psdSendEvent(EHMB_REMDEVICE, pd, NULL);
            (nch->nch_Downstream)[num-1] = NULL;
        }
        /* There's no sense trying to send out commands if the hub is already gone! */
        if(isconnected)
        {
             /* power down for port */
             psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                          USR_CLEAR_FEATURE, UFS_PORT_POWER, (ULONG) num);
             ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
             if(ioerr)
             {
                 psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                "PORT_POWER for port %ld failed: %s (%ld)",
                                num, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                 KPRINTF(1, ("PORT_POWER for port %ld failed %ld!\n", num, ioerr));
             }
        }
    }
    KPRINTF(1, ("FreePipes\n"));
    psdFreePipe(nch->nch_EP1Pipe);
    psdFreePipe(nch->nch_EP0Pipe);
    psdFreeVec(nch->nch_Downstream);
    KPRINTF(1, ("Entering Forbid\n"));
    Forbid();
    // clear queue
    while((msg = GetMsg(nch->nch_CtrlMsgPort)))
    {
        ReplyMsg(msg);
    }
    DeleteMsgPort(nch->nch_TaskMsgPort);
    DeleteMsgPort(nch->nch_CtrlMsgPort);
    CloseLibrary(nch->nch_Base);
    nch->nch_Task = NULL;
    if(nch->nch_ReadySigTask)
    {
        Signal(nch->nch_ReadySigTask, 1L<<nch->nch_ReadySignal);
    }
    KPRINTF(1, ("Really gone now!\n"));
}
/* \\\ */

/* *** HUB Class *** */

/* /// "nClearPortStatus()" */
LONG nClearPortStatus(struct NepClassHub *nch, UWORD port)
{
    LONG ioerr;
    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                 USR_CLEAR_FEATURE, UFS_C_PORT_CONNECTION, (ULONG) port);
    if((ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "CLEAR_PORT_FEATURE (C_PORT_CONNECTION) failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        KPRINTF(10, ("Some error occurred clearing hub status bits!\n"));
        return(ioerr);
    }

    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                 USR_CLEAR_FEATURE, UFS_C_PORT_ENABLE, (ULONG) port);
    if((ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "CLEAR_PORT_FEATURE (C_PORT_ENABLE) failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        KPRINTF(10, ("Some error occurred clearing hub status bits!\n"));
        return(ioerr);
    }

    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                 USR_CLEAR_FEATURE, UFS_C_PORT_SUSPEND, (ULONG) port);
    if((ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "CLEAR_PORT_FEATURE (C_PORT_SUSPEND) failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        KPRINTF(10, ("Some error occurred clearing hub status bits!\n"));
        return(ioerr);
    }

    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                 USR_CLEAR_FEATURE, UFS_C_PORT_OVER_CURRENT, (ULONG) port);
    if((ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0)))
    {
        /*psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "CLEAR_PORT_FEATURE (C_OVER_CURRENT) failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);*/
        KPRINTF(10, ("Some error occurred clearing hub status bits!\n"));
    }

    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                 USR_CLEAR_FEATURE, UFS_C_PORT_RESET, (ULONG) port);
    if((ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0)))
    {
        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                       "CLEAR_PORT_FEATURE (C_PORT_RESET) failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        KPRINTF(10, ("Some error occurred clearing hub status bits!\n"));
        return(ioerr);
    }
    return(0);
}
/* \\\ */

/* /// "nConfigurePort()" */
struct PsdDevice * nConfigurePort(struct NepClassHub *nch, UWORD port)
{
    LONG ioerr;
    LONG delayretries;
    LONG resetretries;
    ULONG delaytime = 10;
    struct UsbPortStatus uhps;
    struct PsdDevice *pd;
    struct PsdPipe *pp;
    BOOL washighspeed = FALSE;
    BOOL islowspeed = FALSE;

    KPRINTF(2, ("Configuring port %ld of hub 0x%08lx\n", port, nch));

    uhps.wPortStatus = 0xDEAD;
    uhps.wPortChange = 0xDA1A;

    /* The whole port bring-up must own the default-address window: on
       snooping legacy-backend HCDs even this first GET_PORT_STATUS reply can
       re-arm a driver's port-to-SET_ADDRESS correlation, so it must not
       interleave with another hub's enumeration. */
    if(!nch->nch_CtxHardware) /* no default-address phase on context HCDs */
        ObtainSemaphore(&nch->nch_HubBase->nh_Adr0Sema);
    psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_CLASS|URTF_OTHER,
                 USR_GET_STATUS, UFS_PORT_CONNECTION, (ULONG) port);
    ioerr = psdDoPipe(nch->nch_EP0Pipe, &uhps, sizeof(struct UsbPortStatus));
    uhps.wPortStatus = AROS_WORD2LE(uhps.wPortStatus);
    uhps.wPortChange = AROS_WORD2LE(uhps.wPortChange);
    if(!ioerr)
    {
    	KPRINTF(2, ("Status 0x%04lx, change 0x%04lx\n", uhps.wPortStatus, uhps.wPortChange));

        if(uhps.wPortStatus & UPSF_PORT_ENABLE)
        {
            KPRINTF(2, ("Disabling port %lu\n", port));

            psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                         USR_CLEAR_FEATURE, UFS_PORT_ENABLE, (ULONG) port);
            ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
            if(ioerr)
            {
                psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                               "CLEAR_PORT_ENABLE failed: %s (%ld)",
                               psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                KPRINTF(1, ("CLEAR_PORT_ENABLE failed %ld.\n", ioerr));
            } else {
                psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                               "Disabling port %ld.", port);
            }
        }
        if(uhps.wPortStatus & UPSF_PORT_CONNECTION)
        {
            KPRINTF(2, ("There's something at port %ld!\n", port));
            Forbid();
            if((pd = psdAllocDevice(nch->nch_Hardware)))
            {
                psdLockWriteDevice(pd);
                Permit();
                /* Hub reference */
                psdSetAttrs(PGA_DEVICE, pd,
                            DA_HubDevice, nch->nch_Device,
                            DA_IsConnected, TRUE,
                            DA_AtHubPortNumber, port,
                            TAG_END);
                if(uhps.wPortStatus & UPSF_PORT_LOW_SPEED)
                {
                    psdSetAttrs(PGA_DEVICE, pd, DA_IsLowspeed, TRUE, TAG_END);
                    KPRINTF(2, ("    It's a lowspeed device!\n"));
                    islowspeed = TRUE;
                }
                for(resetretries = 0; resetretries < 3; resetretries++)
                {
                    psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                 USR_SET_FEATURE, UFS_PORT_RESET, (ULONG) port);
                    ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                    if(ioerr)
                    {
                        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                       "PORT_RESET for port %ld failed: %s (%ld)",
                                           port, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                        KPRINTF(1, ("PORT_RESET failed %ld.\n", ioerr));
                        break;
                    }
                    if(nch->nch_IsRootHub)
                    {
                        // Root hubs need 50ms minimum delay
                        psdDelayMS(50);
                    }
                    for(delayretries = 0; delayretries < 500; delayretries += delaytime)
                    {
                        psdDelayMS(delaytime);
                        psdPipeSetup(nch->nch_EP0Pipe, URTF_IN|URTF_CLASS|URTF_OTHER,
                                     USR_GET_STATUS, UFS_PORT_CONNECTION, (ULONG) port);
                        ioerr = psdDoPipe(nch->nch_EP0Pipe, &uhps, sizeof(struct UsbPortStatus));
                        uhps.wPortStatus = AROS_WORD2LE(uhps.wPortStatus);
                        uhps.wPortChange = AROS_WORD2LE(uhps.wPortChange);
                        if(ioerr)
                        {
                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                           "GET_PORT_CONNECTION for port %ld failed: %s (%ld)",
                                           port, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                            KPRINTF(1, ("GET_PORT_CONNECTION failed %ld.\n", ioerr));
                            break;
                        }
                        KPRINTF(2, ("After reset: status 0x%04lx, change 0x%04lx\n", uhps.wPortStatus, uhps.wPortChange));
                        if(!(uhps.wPortStatus & UPSF_PORT_CONNECTION))
                        {
                            break;
                        }
                        if((uhps.wPortStatus &
                           (UPSF_PORT_RESET|UPSF_PORT_CONNECTION|UPSF_PORT_ENABLE|UPSF_PORT_POWER|UPSF_PORT_OVER_CURRENT)) ==
                           (UPSF_PORT_CONNECTION|UPSF_PORT_ENABLE|UPSF_PORT_POWER))
                        {
                            if((uhps.wPortStatus & UPSF_PORT_HIGH_SPEED) || washighspeed)
                            {
                                psdSetAttrs(PGA_DEVICE, pd, DA_IsHighspeed, TRUE, TAG_END);
                                washighspeed = TRUE;
                                KPRINTF(2, ("    It's a highspeed device!\n"));
                            }
                            else
                            {
                                IPTR needssplit = 0;

				/* Some hubs (Apple Keyboard bultin hub) report speed correctly only after reset */
                                if (uhps.wPortStatus & UPSF_PORT_LOW_SPEED)
                                {
                                    psdSetAttrs(PGA_DEVICE, pd, DA_IsLowspeed, TRUE, TAG_END);
                    		    KPRINTF(2, ("    It's a lowspeed device!\n"));
		                    islowspeed = TRUE;
		                }

                                // inherit needs split from hub
                                psdGetAttrs(PGA_DEVICE, nch->nch_Device, DA_NeedsSplitTrans, &needssplit, TAG_END);
                                KPRINTF(2, ("    Needs split transfers: %ld\n", needssplit));
                                if(nch->nch_IsUSB20) /* this is a low/full speed device connected to a 2.0 hub! */
                                {
				    KPRINTF(2, ("    Enforcing split transfers\n"));
                                    needssplit = TRUE;
                                }
                                psdSetAttrs(PGA_DEVICE, pd, DA_NeedsSplitTrans, needssplit, TAG_END);
                            }
                            nClearPortStatus(nch, port);
                            psdDelayMS((ULONG) (islowspeed ? 1000 : 100));
                            if((pp = psdAllocPipe(pd, nch->nch_TaskMsgPort, NULL)))
                            {
                                if(psdEnumerateDevice(pp))
                                {
                                    KPRINTF(2, ("  Device successfully added!\n"));
                                    psdFreePipe(pp);
                                    psdUnlockDevice(pd);
                                    psdSendEvent(EHMB_ADDDEVICE, pd, NULL);
                                    if(!nch->nch_CtxHardware)
                                        ReleaseSemaphore(&nch->nch_HubBase->nh_Adr0Sema);
                                    nNotifyPeerTwinEvict(nch, port);
                                    return(pd);
                                }
                                psdFreePipe(pp);
                            }
                            break;
                        } else {
                            if(!(uhps.wPortStatus & UPSF_PORT_RESET))
                            {
                                psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                                               "Wrong port status %04lx for port %ld!",
                                               uhps.wPortStatus, port);
                                KPRINTF(2, ("Wrong port status %04lx for port %ld.\n", uhps.wPortStatus, port));
                            }
                        }
                        if(delayretries > 20)
                        {
                            delaytime = 300;
                        }
                    }
                    if((uhps.wPortStatus &
                       (UPSF_PORT_RESET|UPSF_PORT_CONNECTION|UPSF_PORT_ENABLE|UPSF_PORT_POWER|UPSF_PORT_OVER_CURRENT|UPSF_PORT_LOW_SPEED)) ==
                       (UPSF_PORT_CONNECTION|UPSF_PORT_POWER|UPSF_PORT_LOW_SPEED))
                    {
                        psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                       "Strange port response, power-cycling port %ld",
                                       port);
                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                     USR_CLEAR_FEATURE, UFS_PORT_ENABLE, (ULONG) port);
                        ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                        if(ioerr)
                        {
                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                           "CLEAR_PORT_ENABLE for port %ld failed: %s (%ld)",
                                           port, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                            KPRINTF(1, ("CLEAR_PORT_ENABLE for port %ld failed %ld!\n", port, ioerr));
                        }
                        psdDelayMS(50);
                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                     USR_CLEAR_FEATURE, UFS_PORT_POWER, (ULONG) port);
                        ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                        if(ioerr)
                        {
                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                           "CLEAR_PORT_POWER for port %ld failed: %s (%ld)",
                                           port, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                            KPRINTF(1, ("CLEAR_PORT_POWER for port %ld failed %ld!\n", port, ioerr));
                        }
                        psdDelayMS(50);
                        psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                                     USR_SET_FEATURE, UFS_PORT_POWER, (ULONG) port);
                        ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                        if(ioerr)
                        {
                            psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                           "SET_PORT_POWER for port %ld failed: %s (%ld)",
                                           port, psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                            KPRINTF(1, ("SET_PORT_POWER for port %ld failed %ld!\n", port, ioerr));
                        }
                        psdDelayMS((ULONG) nch->nch_PwrGoodTime + 15);

                    }
                    delaytime = 200;
                }
                psdUnlockDevice(pd);
                psdFreeDevice(pd);
                /* Disable port! It's too dangerous having a connection with
                   crazy devices on the bus open */
                psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                             USR_CLEAR_FEATURE, UFS_PORT_ENABLE, (ULONG) port);
                ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
                if(ioerr)
                {
                    psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                                   "CLEAR_PORT_ENABLE failed: %s (%ld)",
                                   psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                    KPRINTF(1, ("CLEAR_PORT_ENABLE failed %ld.\n", ioerr));
                }
                nClearPortStatus(nch, port);
            } else {
                Permit();
                KPRINTF(1, ("AllocDevice() failed.\n"));
            }
        }
    } else {
        psdAddErrorMsg(RETURN_ERROR, (STRPTR) libname,
                       "GET_PORT_CONNECTION failed: %s (%ld)",
                       psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
        KPRINTF(1, ("GET_PORT_CONNECTION failed %ld.\n", ioerr));
    }
    if(!nch->nch_CtxHardware)
        ReleaseSemaphore(&nch->nch_HubBase->nh_Adr0Sema);
    return(NULL);
}
/* \\\ */

/* /// "nHandleHubMethod()" */
void nHandleHubMethod(struct NepClassHub *nch, struct NepHubMsg *nhm)
{
    ULONG num;
    struct PsdDevice *pd;
    nhm->nhm_Result = 0;
    switch(nhm->nhm_MethodID)
    {
        case UCM_HubClaimAppBinding:
            nhm->nhm_Result = (IPTR) psdHubClaimAppBindingA((struct TagItem *) nhm->nhm_Params[1]);
            break;

        case UCM_HubReleaseIfBinding:
        {
            psdHubReleaseIfBinding((struct PsdInterface *) nhm->nhm_Params[1]);
            break;
        }
        case UCM_HubReleaseDevBinding:
            psdHubReleaseDevBinding((struct PsdDevice *) nhm->nhm_Params[1]);
            break;

        case UCM_AttemptSuspendDevice:
        {
            BOOL res = TRUE;
            for(num = 1; num <= nch->nch_NumPorts; num++)
            {
                if((pd = (nch->nch_Downstream)[num-1]))
                {
                    res &= psdSuspendDevice(pd);
                }
            }
            if(res)
            {
                // suspending of all downstream devices successful, so stop all activity, too.
                psdAbortPipe(nch->nch_EP1Pipe);
                nch->nch_Running = FALSE;
                nhm->nhm_Result = TRUE;
            }
            break;
        }

        case UCM_AttemptResumeDevice:
            /* The main loop owns the EP1 pipe: with nch_Running set it will
             * resubmit once the aborted request has drained (nch_IOStarted). */
            nch->nch_Running = TRUE;
            nhm->nhm_Result = TRUE;
            for(num = 1; num <= nch->nch_NumPorts; num++)
            {
                if((pd = (nch->nch_Downstream)[num-1]))
                {
                    psdResumeDevice(pd);
                }
            }
            break;

        case UCM_HubSuspendDevice:
            nhm->nhm_Result = nHubSuspendDevice(nch, (struct PsdDevice *) nhm->nhm_Params[1]);
            break;

        case UCM_HubResumeDevice:
            nhm->nhm_Result = nHubResumeDevice(nch, (struct PsdDevice *) nhm->nhm_Params[1]);
            break;

    }
}
/* \\\ */

/* /// "nHubSuspendDevice()" */
BOOL nHubSuspendDevice(struct NepClassHub *nch, struct PsdDevice *pd)
{
    APTR binding = NULL;
    APTR puc = NULL;
    ULONG num;
    BOOL result = FALSE;
    LONG ioerr;

    psdGetAttrs(PGA_DEVICE, pd,
                DA_Binding, &binding,
                DA_BindingClass, &puc,
                TAG_END);

    for(num = 1; num <= nch->nch_NumPorts; num++)
    {
        if(pd == (nch->nch_Downstream)[num-1])
        {
            psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                         USR_SET_FEATURE, UFS_PORT_SUSPEND, num);

            ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
            if(ioerr)
            {
                psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                               "SET_PORT_SUSPEND failed: %s (%ld)",
                               psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                KPRINTF(1, ("SET_PORT_SUSPEND failed %ld.\n", ioerr));
            } else {
                result = TRUE;
                psdSetAttrs(PGA_DEVICE, pd, DA_IsSuspended, TRUE, TAG_END);
                psdSendEvent(EHMB_DEVSUSPENDED, pd, NULL);
            }
        }
    }
    return result;
}
/* \\\ */

/* /// "nHubResumeDevice()" */
BOOL nHubResumeDevice(struct NepClassHub *nch, struct PsdDevice *pd)
{
    ULONG num;
    BOOL result = FALSE;
    LONG ioerr;

    for(num = 1; num <= nch->nch_NumPorts; num++)
    {
        if(pd == (nch->nch_Downstream)[num-1])
        {
            psdPipeSetup(nch->nch_EP0Pipe, URTF_CLASS|URTF_OTHER,
                         USR_CLEAR_FEATURE, UFS_PORT_SUSPEND, (ULONG) num);

            ioerr = psdDoPipe(nch->nch_EP0Pipe, NULL, 0);
            if(ioerr)
            {
                psdAddErrorMsg(RETURN_WARN, (STRPTR) libname,
                               "CLEAR_PORT_SUSPEND failed: %s (%ld)",
                               psdNumToStr(NTS_IOERR, ioerr, "unknown"), ioerr);
                KPRINTF(1, ("CLEAR_PORT_SUSPEND failed %ld.\n", ioerr));
            } else {
                psdSetAttrs(PGA_DEVICE, pd, DA_IsSuspended, FALSE, TAG_END);
                psdSendEvent(EHMB_DEVRESUMED, pd, NULL);
                result = TRUE;
                psdDelayMS(30);
            }
        }
    }
    return result;
}
/* \\\ */
