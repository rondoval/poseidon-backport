#ifndef POSEIDON_AROS_COMPAT_H
#define POSEIDON_AROS_COMPAT_H
/*
 * Minimal AROS-compatibility shim for building the Poseidon stack against the
 * AmigaOS NDK with bebbo gcc (freestanding, no amiga.lib).  Force-included into
 * every TU (see CMake).
 */

#include <exec/types.h>

/* sfdc's vararg inlines marshal args through a _sfdc_vararg[] array. Default it
   to a pointer type (not ULONG): on bebbo gcc that lets string-literal args pass
   as void* (avoids "wide char array from non-wide string" errors); the residual
   int<->pointer mixing inherent to m68k tag/vararg calls is handled with
   -Wno-int-conversion on the components that use these inlines. */
#ifndef _SFDC_VARARG_DEFINED
#define _SFDC_VARARG_DEFINED
typedef APTR _sfdc_vararg;
#endif

/* --- base types AROS provides but the NDK does not --- */
typedef ULONG IPTR;   /* integer the size of a pointer (32-bit on m68k) */
typedef LONG  SIPTR;  /* signed variant */
typedef APTR  RAWARG; /* pointer to a RawDoFmt-style raw argument array */
typedef void (*VOID_FUNC)();  /* generic code pointer (e.g. Interrupt is_Code casts) */

/* AROS "slow stack format" varargs helpers: get a RAWARG pointer to the varargs that
   follow the last named arg. On m68k they sit on the stack right after it, so ARG = &x+1. */
#define AROS_SLOWSTACKFORMAT_PRE(x)
#define AROS_SLOWSTACKFORMAT_ARG(x)  ((RAWARG)(&(x) + 1))
#define AROS_SLOWSTACKFORMAT_POST(x)

/* AROS/SAS-C case-insensitive string compares. Self-contained (not libc strcasecmp,
   which drags in malloc.o -> an unresolved SysBase in our freestanding link). */
static inline int _ci_lc(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static inline int stricmp(const char *a, const char *b)
{
    int ca, cb;
    do { ca = _ci_lc((UBYTE)*a++); cb = _ci_lc((UBYTE)*b++); } while(ca && ca == cb);
    return ca - cb;
}
static inline int strnicmp(const char *a, const char *b, ULONG n)
{
    int ca = 0, cb = 0;
    while(n-- && (ca = _ci_lc((UBYTE)*a++)) == (cb = _ci_lc((UBYTE)*b++)) && ca) ;
    return ca - cb;
}

/* --- byte order ---------------------------------------------------------
 * m68k is BIG-endian, so the *BE* conversions are identity and the *LE*
 * conversions byte-swap.  USB descriptors are little-endian, so AROS_LE2WORD
 * etc. MUST swap; IFF chunk IDs/lengths are big-endian (AROS_LONG2BE identity).
 */
#define AROS_BE2LONG(x) ((ULONG)(x))
#define AROS_LONG2BE(x) ((ULONG)(x))
#define AROS_BE2WORD(x) ((UWORD)(x))
#define AROS_WORD2BE(x) ((UWORD)(x))
#define AROS_LE2LONG(x) ((ULONG)__builtin_bswap32((ULONG)(x)))
#define AROS_LONG2LE(x) ((ULONG)__builtin_bswap32((ULONG)(x)))
#define AROS_LE2WORD(x) ((UWORD)__builtin_bswap16((UWORD)(x)))
#define AROS_WORD2LE(x) ((UWORD)__builtin_bswap16((UWORD)(x)))

/* --- misc AROS portability knobs --- */
#define AROS_WORSTALIGN 8       /* worst-case data alignment for the mem pool */
#define AROS_STACKSIZE  16384   /* default spawned-subtask stack size */

/* AROS-only CreatePool flag for semaphore-protected pools. AmigaOS pools aren't
   intrinsically MT-safe; Poseidon does its own locking, so this is a no-op. */
#ifndef MEMF_SEM_PROTECTED
#define MEMF_SEM_PROTECTED 0
#endif

/* --- AROS list-iteration macros --- */
#ifndef ForeachNode
#define ForeachNode(list, node) \
  for (node = (void *)(((struct List *)(list))->lh_Head); \
       ((struct Node *)(node))->ln_Succ; \
       node = (void *)(((struct Node *)(node))->ln_Succ))
#endif
#ifndef ForeachNodeSafe
#define ForeachNodeSafe(list, node, nextnode) \
  for (node = (void *)(((struct List *)(list))->lh_Head); \
       (nextnode = (void *)(((struct Node *)(node))->ln_Succ)); \
       node = (void *)nextnode)
#endif

/* PKCTRL_IPTR: AROS pack-control for pointer-sized fields. On 32-bit m68k an
   IPTR is a LONG, so it packs identically to PKCTRL_LONG. */
#ifndef PKCTRL_IPTR
#define PKCTRL_IPTR PKCTRL_LONG
#endif

/* BNULL: AROS/OS4 typed BPTR-null. The 3.2 NDK only has plain 0. */
#ifndef BNULL
#define BNULL ((BPTR)0)
#endif

/* --- exec NewList without amiga.lib (we link freestanding) --- */
#ifndef NEWLIST
#define NEWLIST(_l) ((void)( \
    ((struct List *)(_l))->lh_Head     = (struct Node *)&((struct List *)(_l))->lh_Tail, \
    ((struct List *)(_l))->lh_Tail     = (struct Node *)0, \
    ((struct List *)(_l))->lh_TailPred = (struct Node *)&((struct List *)(_l))->lh_Head ))
#endif

#endif /* POSEIDON_AROS_COMPAT_H */
