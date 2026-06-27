/* debug.h — shared compile-time debug macros for all Poseidon USB class drivers.
 *
 * Build with -DDEBUG=<level> to enable KPRINTF()/DB() logging (routed through
 * KPrintF from debug.lib; the NDK has no <proto/debug.h>). DEBUG unset or 0 = no
 * code emitted. Paired with the shared classes/debug.c (dumpmem).
 */
#ifndef __DEBUG_H__
#define __DEBUG_H__

#undef KPRINTF
#undef DB

#ifndef DEBUG
#define DEBUG 0
#endif

// DEBUG 0 should equal undefined DEBUG
#ifdef DEBUG
#if DEBUG == 0
#undef DEBUG
#endif
#endif

// KPrintF prototype only when logging (NDK has no <proto/debug.h>).
#ifdef DEBUG
#include <clib/debug_protos.h>
#endif

#ifdef DEBUG
#ifndef DB_LEVEL
#define DB_LEVEL 1
#endif
#define KPRINTF(l, x) do { if ((l) >= DB_LEVEL) \
     { KPrintF("%s:%s/%lu: ", __FILE__, __FUNCTION__, __LINE__); KPrintF x;} } while (0)
#define DB(x) x
   void dumpmem(void *mem, unsigned long int len);
#else /* !DEBUG */

#define KPRINTF(l, x) ((void) 0)
#define DB(x)

#endif /* DEBUG */

#endif /* __DEBUG_H__ */
