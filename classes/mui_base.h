/* mui_base.h — shared ROM-safe MUI base accessor for the USB class drivers.
 *
 * Why a file-scope accessor (not a local, not a global): MUI's vararg object
 * constructors (the `…End` idiom) are __inline functions compiled at include time
 * that resolve MUIMASTER_BASE_NAME at *file* scope — a per-function local base is
 * invisible to them, and `#define NO_INLINE_STDARG` to dodge that just drops the
 * constructors (undefined MUI_NewObject/MUI_MakeObject at link). A writable global
 * base would block ROM-residency. So we recover the libbase from the running task:
 * every MUI call runs in the GUI subtask, spawned (psdSpawnSubTask) with the
 * instance/libbase in tc_UserData.
 */
#ifndef MUI_BASE_USERDATA
#error "#define MUI_BASE_USERDATA (the tc_UserData struct) before including mui_base.h"
#endif
#ifndef MUI_BASE_FIELD
#error "#define MUI_BASE_FIELD (the *_MUIBase field) before including mui_base.h"
#endif

#include <exec/execbase.h>
#include <exec/tasks.h>

/* EXEC_BASE_NAME is the $4 exec-base macro from common.h / class_main.c (no global). */
static inline struct Library *_mui_base(void)
{
    return ((MUI_BASE_USERDATA *)EXEC_BASE_NAME->ThisTask->tc_UserData)->MUI_BASE_FIELD;
}
#define MUIMASTER_BASE_NAME (_mui_base())

#include <proto/muimaster.h>

/* The SDK's __inline MUI_NewObject is miscompiled under bebbo -O2.
 * Shadow it with a va_list version. Included HERE, after MUIMASTER_BASE_NAME is bound
 * to (_mui_base()) and <proto/muimaster.h> is in, so the replacement resolves
 * the per-instance base. */
#include "mui_newobject_fix.h"

#undef MUI_BASE_USERDATA
#undef MUI_BASE_FIELD
