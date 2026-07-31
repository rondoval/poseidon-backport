/* common.h — shared include aggregator for all Poseidon USB class drivers.
 *
 * Every class compiles the shared classes/class_main.c skeleton and pulls in
 * this header for the common NDK/Poseidon includes. Per-class identity comes
 * from the class's CMakeLists via -D: the source uses CLASS_NAME directly, plus
 * VERSION_STRING — the $VER cookie built from it and the distribution version.
 */

#include "class_version.h"

#include <exec/types.h>
#include <exec/lists.h>
#include <exec/alerts.h>
#include <exec/memory.h>
#include <exec/libraries.h>
#include <exec/interrupts.h>
#include <exec/semaphores.h>
#include <exec/execbase.h>
#define EXEC_BASE_NAME (*(struct ExecBase **)4UL)
#include <exec/devices.h>
#include <exec/io.h>
#include <exec/ports.h>
#include <exec/errors.h>
#include <exec/resident.h>
#include <exec/initializers.h>

#include <devices/timer.h>
#include <devices/input.h>
#include <utility/utility.h>
#include <dos/dos.h>
#include <intuition/intuition.h>

#include <devices/usb.h>
#include <devices/usbhardware.h>
#include <libraries/usbclass.h>

#include <string.h>
#include <stddef.h>
#include <stdio.h>

#include <proto/alib.h>
#include <proto/dos.h>
#include <proto/commodities.h>
#include <proto/intuition.h>
#include <proto/poseidon.h>
#include <proto/utility.h>
#include <proto/keymap.h>
#include <proto/layers.h>
#include <proto/input.h>
#include <proto/expansion.h>
#include <proto/exec.h>

#define NewList NEWLIST

#include <stdarg.h>

#define min(x,y) (((x) < (y)) ? (x) : (y))
#define max(x,y) (((x) > (y)) ? (x) : (y))

/* Plain wordings shared by every class driver, for psdTxt() (see
 * <libraries/poseidon.h>). Each class keeps its own traditional wording at its
 * own call site and passes it in as `flavour`; only the boring half is common.
 * The plain text is lan78xx.class's, which was already written this way.
 *
 * The specifier-prefix rule applies: the args listed below are what the call
 * site must pass, in that order.
 */
#define PSD_BOUND_TXT(flavour)    psdTxt("Bound '%s' to %s unit %ld.", (flavour))  /* devname, unit name, unit no */
#define PSD_BOUND1_TXT(flavour)   psdTxt("Bound '%s'.", (flavour))                 /* devname */
#define PSD_RELEASED_TXT(flavour) psdTxt("Released '%s'.", (flavour))              /* devname */
#define PSD_GIVEUP_TXT            psdTxt("Too many consecutive transfer errors; shutting the device down.", \
                                         "That's it, that device pissed me off long enough!")
#define PSD_OK_TXT(flavour)       psdTxt("OK", (flavour))                          /* requester gadget */
