/* mui_newobject_fix.h — replace the MUI SDK's broken MUI_NewObject().
 *
 * The MUI 5 SDK's <inline/muimaster.h> defines:
 *     __inline Object *MUI_NewObject(CONST_STRPTR cl, Tag tags, ...)
 *     { return MUI_NewObjectA(cl, (struct TagItem *)&tags); }
 * i.e. it passes the ADDRESS OF THE FIRST NAMED VARARG as the tag array. Under bebbo
 * gcc -O2 that doesn't work. This bites Window, Scrollgroup, Listview, and
 * custom List/Group subclasses (verified: a bare WindowObject returns NULL with the SDK
 * inline, a valid object with this fix).
 *
 * Fix: provide a correct replacement that uses va_list (which locates the varargs
 * properly) and rename MUI_NewObject to it with an OBJECT-LIKE macro — so the SDK's
 * XxxObject tree macros keep expanding to a plain function call.
 *
 * Force-include this; it pulls <proto/muimaster.h> first so the SDK inline is defined
 * before we shadow the name (order matters — the reverse would corrupt the inline).
  */
#ifndef PSD_MUI_NEWOBJECT_FIX_H
#define PSD_MUI_NEWOBJECT_FIX_H

#include <proto/muimaster.h>     /* MUI_NewObjectA + the (broken) __inline MUI_NewObject */
#include <libraries/mui.h>       /* the XxxObject macros that expand to MUI_NewObject(...) */
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

#endif /* PSD_MUI_NEWOBJECT_FIX_H */
