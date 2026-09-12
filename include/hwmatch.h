#ifndef POSEIDON_HWMATCH_H
#define POSEIDON_HWMATCH_H
/*
 * One definition of "these two names mean the same host controller".
 *
 * A Poseidon hardware entry is identified by a device name plus a unit number,
 * but the name is not written down consistently: the ROM startup resident adds
 * the bare "xhci.device" (so it resolves against a Kickstart-resident device
 * with no DEVS: hit), Trident's own default is "DEVS:USBHardware/xhci.device",
 * and a saved poseidon.prefs keeps whatever spelling was current when it was
 * written. The library's pFindHardware() compares through this header too, so
 * everything inside and outside the library agrees by construction.
 */

#include <exec/types.h>
#include <string.h>     /* stricmp() */

/* The trailing path component: what follows the last '/' or ':'. Same rule the
   library's own OpenDevice()/OpenLibrary() retry walks use, and the reason a
   stored path name and a bare name reach the same driver in the first place. */
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
