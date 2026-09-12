/* mui_compat.h — the two things every MUI translation unit needs.
 *
 * (1) A WORKING MUI_NewObject. The MUI 5 SDK's <inline/muimaster.h> defines:
 *         __inline Object *MUI_NewObject(CONST_STRPTR cl, Tag tags, ...)
 *         { return MUI_NewObjectA(cl, (struct TagItem *)&tags); }
 *     i.e. it takes the address of the first NAMED vararg and assumes the rest follow
 *     contiguously. The `...' args are never read via va_arg, so the inliner is free to
 *     drop them as dead — and does: a 5-attribute WindowObject compiles to a single
 *     stored tag with no values and no TAG_DONE, and MUI walks off the end. This is not
 *     a miscompile to be waited out, it is invalid C; it breaks identically at -O1, -O2,
 *     -O3 and -Os (only -O0 survives, which is why it looks like an optimizer bug).
 *     See porting-playbook.md §4.1 for the disassembly.
 *
 *     Fix: a va_list replacement, shadowed by an OBJECT-LIKE macro so the SDK's
 *     XxxObject tree macros keep expanding to a plain call. Object-like is forced: a
 *     function-like macro needs its closing `)', which lives inside End's expansion and
 *     is invisible while arguments are collected.
 *
 * (2) The MUI 3.8 runtime floor. We compile against the MUI 5 SDK (the only one bebbo's
 *     gcc can parse) but run on muimaster.library 19 and up. The SDK's own
 *     MUIMASTER_VMIN is 20, which would refuse a v19 library, so it is shadowed below
 *     rather than edited at the 25 OpenLibrary sites — a new GUI class copied from an
 *     existing one then cannot quietly regress the floor.
 *
 * Force-included into every MUI TU: Trident via its CMake -include, the class drivers
 * via classes/mui_base.h, popo.gui.c directly. Order matters — <proto/muimaster.h> is
 * pulled first, so the SDK inline exists before we shadow the name, and MUI_NewObjectA
 * binds to whatever MUIMASTER_BASE_NAME is at that point (the global MUIMasterBase for
 * Trident, the per-instance task accessor for the classes and popo).
 */
#ifndef PSD_MUI_COMPAT_H
#define PSD_MUI_COMPAT_H

#include <proto/muimaster.h>     /* MUI_NewObjectA + the (broken) __inline MUI_NewObject */
#include <libraries/mui.h>       /* the XxxObject macros, and the SDK's MUIMASTER_VMIN */
#include <exec/libraries.h>      /* struct Library, for psd_MUIVersion() */
#include <stdarg.h>

static __inline Object *psd_MUI_NewObject(CONST_STRPTR cl, ...)
{
    Object *o;
    va_list va;
    va_start(va, cl);
    o = MUI_NewObjectA(cl, (struct TagItem *) va);
    va_end(va);
    return o;
}

#undef MUI_NewObject
#define MUI_NewObject psd_MUI_NewObject

#undef MUIMASTER_VMIN
#define MUIMASTER_VMIN 19        /* 19 = MUI 3.8, 20 = MUI 4.0, 21 = MUI 5.x */

/* The running muimaster version, from the base this TU already resolves. ROM-clean: no
 * state of its own, and no library call — MUIMASTER_BASE_NAME is either the global or an
 * inlined tc_UserData read. */
static __inline UWORD psd_MUIVersion(void)
{
    return ((struct Library *) (MUIMASTER_BASE_NAME))->lib_Version;
}

#endif /* PSD_MUI_COMPAT_H */
 