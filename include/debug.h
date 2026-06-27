/* debug.h — stack-wide debug logging for the Poseidon backport.
 *
 * Header-only: the KPRINTF()/XPRINTF()/DB() macros AND the formatter live here as
 * static inlines (no shared .c). The output backend is chosen at build time by
 * cmake/PoseidonDebug.cmake (POSEIDON_DEBUG_BACKEND):
 *
 *   pistorm : RawDoFmt -> *(UBYTE*)0xdeadbeef, which Emu68/PiStorm traps and prints
 *             on the Pi console. No debug.lib.                       (default)
 *   serial  : RawDoFmt -> KPutChar (debug.lib) -> serial @ 9600.     (-DDEBUG_SERIAL)
 *   off      : DEBUG undefined -> all logging compiled out.
 *
 * DEBUG is simply defined (logging on) or not (off) by the backend — it carries no
 * level. Verbosity is DB_LEVEL: KPRINTF(level, x) emits iff level >= DB_LEVEL, where
 * level is the message priority (1 = trace ... 200 = critical) and DB_LEVEL (default
 * 1 = show all) comes from cmake via POSEIDON_DEBUG_LEVEL. The call-site API is
 * unchanged from the old per-component debug.h's; only the output path changed
 * (debug.lib KPrintF -> exec RawDoFmt + a switchable byte sink).
 */
#ifndef POSEIDON_DEBUG_H
#define POSEIDON_DEBUG_H

#undef KPRINTF
#undef XPRINTF
#undef DB

#ifdef DEBUG

#include <stdarg.h>
/* RawDoFmt is an exec call; bind its inline to the canonical $4 base so this header
 * is self-contained under __NOLIBBASE__ (classes/poseidon) regardless of include
 * order. Matches classes/common.h's identical definition (no redefinition). */
#ifndef EXEC_BASE_NAME
#define EXEC_BASE_NAME (*(struct ExecBase **)4UL)
#endif
#include <proto/exec.h>
#ifdef DEBUG_SERIAL
#include <clib/debug_protos.h>   /* KPutChar (debug.lib); NDK has no <proto/debug.h> */
#endif

#ifndef DB_LEVEL
#define DB_LEVEL 1
#endif

/* RawDoFmt byte sink: data in d0, our putChData (NULL) in a3 — same ABI as
 * poseidon.library.c's pPutChar(). volatile keeps the MMIO store from being elided. */
static inline void psd_putch(UBYTE data asm("d0"), APTR dummy asm("a3"))
{
    (void)dummy;
    if (data != 0)
    {
#ifdef DEBUG_SERIAL
        KPutChar(data);
#else
        *(volatile UBYTE *)0xdeadbeef = data;
#endif
    }
}

/* Shared formatter. %p is NOT supported by exec RawDoFmt — debug call sites use
 * %08lx for pointers instead (see the build's %p migration). */
static inline void psd_kprintf(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    RawDoFmt((CONST_STRPTR)fmt, (APTR)args, (APTR)psd_putch, NULL);
    va_end(args);
}

#define KPRINTF(l, x) do { if ((l) >= DB_LEVEL) \
     { psd_kprintf("%s:%s/%lu: ", __FILE__, __FUNCTION__, __LINE__); psd_kprintf x; } } while (0)
#define XPRINTF(l, x) KPRINTF(l, x)
#define DB(x) x

static inline void dumpmem(void *mem, unsigned long int len)
{
    unsigned char *p;
    if (!mem || !len) { return; }
    p = (unsigned char *) mem;
    psd_kprintf("\n");
    do
    {
        unsigned char b, c, str[17];
        for (b = 0; b < 16; b++)
        {
            c = *p++;
            str[b] = ((c >= ' ') && (c <= 'z')) ? c : '.';
            str[b + 1] = 0;
            psd_kprintf("%02lx ", c);
            if (--len == 0) break;
        }
        while (++b < 16) { psd_kprintf("   "); }
        psd_kprintf("  %s\n", str);
    } while (len);
    psd_kprintf("\n\n");
}

#else /* !DEBUG */

#define KPRINTF(l, x) ((void) 0)
#define XPRINTF(l, x) ((void) 0)
#define DB(x)

#endif /* DEBUG */

#endif /* POSEIDON_DEBUG_H */
