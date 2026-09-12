#ifndef POSEIDON_HWMATCH_H
#define POSEIDON_HWMATCH_H
/*
 * One definition of host-controller naming, shared by the library, Trident and
 * the CLI tools.
 *
 * A Poseidon hardware entry is a device name plus a unit number, and the name
 * is always the BARE driver name ("xhci.device"): psdAddHardware() strips any
 * path, so phw_DevName / HA_DeviceName, Trident's list and the prefs Trident
 * writes never carry one. Where the driver comes from is decided when it is
 * opened: one already in memory (Kickstart or Emu68 ROM resident, or opened
 * before) by that name, anything else from PSD_HWDRAWER -- a bare name alone
 * would only ever be looked for in DEVS:.
 *
 * Paths still arrive from outside: an older poseidon.prefs, a command line, an
 * ASL pick. So matching compares trailing path components, and a stored
 * "DEVS:USBHardware/xhci.device" and a live "xhci.device" stay one controller.
 */

#include <exec/types.h>
#include <string.h>     /* stricmp() */

/* The one drawer host-controller drivers are loaded from. */
#define PSD_HWDRAWER "DEVS:USBHardware"

/* The trailing path component: what follows the last '/' or ':'. */
static inline CONST_STRPTR psdHwFilePart(CONST_STRPTR s)
{
    CONST_STRPTR part = s;

    while(*s)
    {
        if((*s == '/') || (*s == ':'))
        {
            part = s + 1;
        }
        s++;
    }
    return(part);
}

/* TRUE if both names denote the same device. Either side may carry a path. */
static inline BOOL psdHwNameMatch(CONST_STRPTR a, CONST_STRPTR b)
{
    if(!a || !b)
    {
        return(FALSE);
    }
    return((BOOL) (stricmp((const char *) psdHwFilePart(a),
                           (const char *) psdHwFilePart(b)) == 0));
}

/* TRUE if both name/unit pairs denote the same hardware entry. */
static inline BOOL psdHwMatch(CONST_STRPTR a, ULONG unita,
                              CONST_STRPTR b, ULONG unitb)
{
    return((BOOL) ((unita == unitb) && psdHwNameMatch(a, b)));
}

#endif /* POSEIDON_HWMATCH_H */
