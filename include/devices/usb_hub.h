#ifndef DEVICES_USB_HUB_H
#define DEVICES_USB_HUB_H
/*
**	$VER: usb_hub.h 3.1 (06.04.23)
**
**	usb definitions include file
**
**	(C) Copyright 2002-2009 Chris Hodges
**	    All Rights Reserved
*/

#include <exec/types.h>

#if defined(__GNUC__)
# pragma pack(1)
#endif

/* Usb Hub Requests */
#define UHR_GET_STATE         0x02 /* URTF_OTHER for port and URTF_DEVICE for Hub itself */
#define UHR_CLEAR_TT_BUFFER   0x08
#define UHR_RESET_TT_BUFFER   0x09
#define UHR_GET_TT_STATE      0x0a
#define UHR_STOP_TT           0x0b
#define UHR_SET_HUB_DEPTH     0x0c  /* USB 3.x: wValue = hub tier (0 = attached to root port) */
#define UHR_GET_PORT_ERR_COUNT 0x0d /* USB 3.x */

/* Usb Hub Feature Selectors */
#define UFS_C_HUB_LOCAL_POWER     0x00
#define UFS_C_HUB_OVER_CURRENT    0x01
#define UFS_PORT_CONNECTION       0x00
#define UFS_PORT_ENABLE           0x01
#define UFS_PORT_SUSPEND          0x02
#define UFS_PORT_OVER_CURRENT     0x03
#define UFS_PORT_RESET            0x04
#define UFS_PORT_POWER            0x08
#define UFS_PORT_LOW_SPEED        0x09
#define UFS_C_PORT_CONNECTION     0x10
#define UFS_C_PORT_ENABLE         0x11
#define UFS_C_PORT_SUSPEND        0x12
#define UFS_C_PORT_OVER_CURRENT   0x13
#define UFS_C_PORT_RESET          0x14
#define UFS_PORT_TEST             0x15
#define UFS_PORT_INDICATOR        0x16

/* USB 3.x hub feature selectors (USB 3.2 spec table 10-9) */
#define UFS_PORT_LINK_STATE       0x05 /* SET: wIndex bits 11:8 = target link state (0=U0, 3=U3) */
#define UFS_PORT_U1_TIMEOUT       0x17
#define UFS_PORT_U2_TIMEOUT       0x18
#define UFS_C_PORT_LINK_STATE     0x19
#define UFS_C_PORT_CONFIG_ERROR   0x1a
#define UFS_PORT_REMOTE_WAKE_MASK 0x1b
#define UFS_BH_PORT_RESET         0x1c
#define UFS_C_BH_PORT_RESET       0x1d
#define UFS_FORCE_LINKPM_ACCEPT   0x1e

/* USB 3.x link states (UPLS_*) — used both as the SET target for
   UFS_PORT_LINK_STATE (wIndex bits 11:8) and as the value read back in the
   wPortStatus link-state field (bits 8:5). Values are the raw PLS numbers. */
#define UPLS_U0                   0
#define UPLS_U1                   1
#define UPLS_U2                   2
#define UPLS_U3                   3
#define UPLS_SS_DISABLED          4
#define UPLS_RX_DETECT            5
#define UPLS_SS_INACTIVE          6
#define UPLS_POLLING              7
#define UPLS_RECOVERY             8
#define UPLS_HOT_RESET            9
#define UPLS_COMPLIANCE           10
#define UPLS_TEST_MODE            11
#define UPLS_RESUME               15

/* HUB class specific descriptors */
#define UDT_HUB               0x29
#define UDT_SSHUB             0x2a  /* SuperSpeed hub descriptor */

/* Usb Class Specific Descriptor: Hub Descriptor */
struct  UsbHubDesc
{
    UBYTE bLength;             /* Number of bytes in this descriptor, including this byte */
    UBYTE bDescriptorType;     /* Descriptor Type, value:  29H for hub descriptor */
    UBYTE bNbrPorts;           /* Number of downstream ports that this hub supports */
    UWORD wHubCharacteristics; /* Hub flags */
    UBYTE bPwrOn2PwrGood;      /* Time (in 2ms intervals) for power-good on port */
    UBYTE bHubContrCurrent;    /* Maximum current requirements of the Hub Controller in mA. */
    UBYTE DeviceRemovable;     /* Variable Size! Indicates if a port has a removable (0) device attached, Bit n<-> Port n */
    UBYTE PortPwrCtrlMask;     /* Variable Size! Obsolete (USB1.0) */
};

/* Usb Class Specific Descriptor: SuperSpeed Hub Descriptor */
struct  UsbSSHubDesc
{
    UBYTE bLength;             /* Number of bytes in this descriptor, including this byte */
    UBYTE bDescriptorType;     /* Descriptor Type, value:  2AH for SuperSpeed hub descriptor */
    UBYTE bNbrPorts;           /* Number of downstream ports that this hub supports */
    UWORD wHubCharacteristics; /* Hub flags */
    UBYTE bPwrOn2PwrGood;      /* Time (in 2ms intervals) for power-good on port */
    UBYTE bHubContrCurrent;    /* */
    UBYTE bHubHdrDecLat;       /* Hub Packet Header Decode Latency */
    UWORD wHubDelay;           /* */
    UWORD DeviceRemovable;     /* Indicates if a port has a removable device attached */
};

/* Flags for wHubCharacteristics */
#define UHCF_INDIVID_POWER    0x0001 /* Individual port power switching */
#define UHCF_IS_COMPOUND      0x0004 /* Hub is part of a compound device */
#define UHCF_INDIVID_OVP      0x0008 /* Individual port over-current status */
#define UHCF_NO_OVP           0x0010 /* No over-current protection */
#define UHCF_PORT_INDICATORS  0x0080 /* Port indicators are supported */

#define UHCS_THINK_TIME       13
#define UHCF_THINK_TIME_8     0x0000 /* TT Think Time 8 FS bit times */
#define UHCF_THINK_TIME_16    0x2000 /* TT Think Time 16 FS bit times */
#define UHCF_THINK_TIME_24    0x4000 /* TT Think Time 24 FS bit times */
#define UHCF_THINK_TIME_32    0x6000 /* TT Think Time 32 FS bit times */
#define UHCM_THINK_TIME       0x6000

/* Structure returned by GetHubStatus() */

struct UsbHubStatus
{
    UWORD wHubStatus;          /* Current status of hub (see below) */
    UWORD wHubChange;          /* Changes of status */
};

/* Flags for wHubStatus and wHubChange */
#define UHSF_LOCAL_POWER_LOST 0x0001
#define UHSF_OVER_CURRENT     0x0002

/* Structure returned by GetPortStatus() */
struct UsbPortStatus
{
    UWORD wPortStatus;         /* Current status of port (see below) */
    UWORD wPortChange;         /* Changes of status */
};

/* Flags for wPortStatus.
   Bits 0/1/3/4 are common to USB 2.0 and USB 3.x; bits 8+ are USB 2.0 only and
   are reused with different meaning by the USB 3.x SuperSpeed layout (UPSF_SS_*). */
#define UPSF_PORT_CONNECTION        0x0001 /* common */
#define UPSF_PORT_ENABLE            0x0002 /* common */
#define UPSF_PORT_SUSPEND           0x0004 /* USB 2.0 only (USB 3.x: link state U3) */
#define UPSF_PORT_OVER_CURRENT      0x0008 /* common */
#define UPSF_PORT_RESET             0x0010 /* common */
#define UPSF_PORT_POWER             0x0100 /* USB 2.0 only (USB 3.x power is UPSF_SS_PORT_POWER, bit 9) */
#define UPSF_PORT_LOW_SPEED         0x0200 /* USB 2.0 only (bit reused by UPSF_SS_PORT_POWER) */
#define UPSF_PORT_HIGH_SPEED        0x0400 /* USB 2.0 only (bit reused by UPSF_SS_PORT_SPEED) */
#define UPSF_PORT_TEST_MODE         0x0800 /* USB 2.0 only (bit reused by UPSF_SS_PORT_SPEED) */
#define UPSF_PORT_INDICATOR         0x1000 /* USB 2.0 only (bit reused by UPSF_SS_PORT_SPEED) */

/* USB 3.x wPortStatus — SuperSpeed layout (USB 3.2 Table 10-10).
   CONNECTION/ENABLE/OVER_CURRENT/RESET reuse the UPSF_PORT_* bits above. */
#define UPSF_SS_PORT_LINK_STATE     0x01e0 /* bits 8:5 = current link state (UPLS_*) */
#define UPSS_SS_PORT_LINK_STATE     5      /* shift to extract the link-state field */
#define UPSF_SS_PORT_POWER          0x0200 /* bit 9 (NB: USB2 power is UPSF_PORT_POWER 0x0100) */
#define UPSF_SS_PORT_SPEED          0x1c00 /* bits 12:10 = negotiated speed (0 = 5 Gbps) */
#define UPSS_SS_PORT_SPEED          10

/* Flags for wPortChange (USB 2.0 Table 11-22 + USB 3.2 Table 10-12).
   Bits 0/3/4 common; 1/2 USB 2.0 only; 5/6/7 USB 3.x only. */
#define UPCF_C_PORT_CONNECTION      0x0001 /* common */
#define UPCF_C_PORT_ENABLE          0x0002 /* USB 2.0 only */
#define UPCF_C_PORT_SUSPEND         0x0004 /* USB 2.0 only */
#define UPCF_C_PORT_OVER_CURRENT    0x0008 /* common */
#define UPCF_C_PORT_RESET           0x0010 /* common */
#define UPCF_C_BH_PORT_RESET        0x0020 /* USB 3.x only */
#define UPCF_C_PORT_LINK_STATE      0x0040 /* USB 3.x only */
#define UPCF_C_PORT_CONFIG_ERROR    0x0080 /* USB 3.x only */

#if defined(__GNUC__)
# pragma pack()
#endif

#endif /* DEVICES_USB_HUB_H */
