/*
 * poseidon.library — library skeleton (romtag, init/open/close/expunge, LVO
 * funcTable). Hand-written replacement for AROS genmodule. Bebbo gcc, freestanding.
 *
 * The real init/open/expunge logic lives in poseidon.library.c (libInit/libOpen/
 * libExpunge); this file only wraps them in the standard Exec library vectors and
 * publishes the 96-entry jump table in poseidon.conf/poseidon.sfd order.
 */

#include <exec/types.h>
#include <exec/resident.h>
#include <exec/libraries.h>
#include <exec/memory.h>
#include <exec/execbase.h>

#include <proto/exec.h>

#include "poseidon_intern.h"          /* struct PsdBase */
#include <clib/poseidon_protos.h>     /* the 96 psd* prototypes (for funcTable) */

#define LIBRARY_VERSION  5
#define LIBRARY_REVISION 3
#define LIBRARY_PRIORITY 48

/* SysBase: referenced as 'extern' by poseidon.library.c; defined+owned here. */
struct ExecBase *SysBase;

extern const char libname[];          /* "poseidon.library", in poseidon.library.c */
static const char libIdString[] = "$VER: poseidon.library 5.3 (25.06.2026)";

/* the real hooks (in poseidon.library.c) */
extern int libInit(struct PsdBase *ps);
extern int libOpen(struct PsdBase *ps);
extern int libExpunge(struct PsdBase *ps);

/* forward decls */
extern const UBYTE endOfCode;
static const APTR initTable[4];

/* Refuse to run if someone tries to execute the library as a program. */
LONG __attribute__((used)) doNotExecute(void);
LONG __attribute__((used)) doNotExecute(void) { return -1; }

static void freeBase(struct PsdBase *base)
{
    ULONG size = (ULONG)base->ps_Library.lib_NegSize + base->ps_Library.lib_PosSize;
    FreeMem((APTR)((ULONG)base - base->ps_Library.lib_NegSize), size);
}

static struct PsdBase *LibInit(struct PsdBase *base   asm("d0"),
                               BPTR            seglist asm("a0"),
                               struct ExecBase *sysbase asm("a6"))
{
    SysBase = sysbase;
    base->ps_SegList = seglist;
    base->ps_Library.lib_Revision = LIBRARY_REVISION;

    if(libInit(base))
        return base;

    freeBase(base);
    return NULL;
}

static struct PsdBase *LibOpen(struct PsdBase *base asm("a6"))
{
    base->ps_Library.lib_OpenCnt++;
    base->ps_Library.lib_Flags &= ~LIBF_DELEXP;
    if(!libOpen(base)) {
        base->ps_Library.lib_OpenCnt--;
        return NULL;
    }
    return base;
}

static BPTR LibExpunge(struct PsdBase *base asm("a6"))
{
    BPTR seglist;
    if(base->ps_Library.lib_OpenCnt > 0) {
        base->ps_Library.lib_Flags |= LIBF_DELEXP;
        return 0;
    }
    seglist = base->ps_SegList;
    libExpunge(base);                 /* tears down + Remove()s the lib node */
    freeBase(base);
    return seglist;
}

static BPTR LibClose(struct PsdBase *base asm("a6"))
{
    if(--base->ps_Library.lib_OpenCnt == 0 &&
       (base->ps_Library.lib_Flags & LIBF_DELEXP))
        return LibExpunge(base);
    return 0;
}

static ULONG LibNull(void) { return 0; }

/* The LVO jump table. Order IS the ABI: 4 std vectors, then the 96 functions
   in poseidon.conf/poseidon.sfd order, then the -1 terminator. */
static const APTR funcTable[] = {
    (APTR)LibOpen,
    (APTR)LibClose,
    (APTR)LibExpunge,
    (APTR)LibNull,
#include "poseidon_funcs.inc"
    (APTR)-1
};

static const APTR initTable[4] = {
    (APTR)sizeof(struct PsdBase),
    (APTR)funcTable,
    (APTR)0,
    (APTR)LibInit
};

const struct Resident romTag __attribute__((used)) = {
    RTC_MATCHWORD,
    (struct Resident *)&romTag,
    (APTR)&endOfCode,
    RTF_AUTOINIT,
    LIBRARY_VERSION,
    NT_LIBRARY,
    LIBRARY_PRIORITY,
    (char *)libname,
    (char *)libIdString,
    (APTR)initTable
};
