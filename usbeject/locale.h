#ifndef _LOCALE_H
#define _LOCALE_H

#include <exec/types.h>

#define CATCOMP_NUMBERS
#include "strings.h"

CONST_STRPTR _(ULONG ID);       /* Get a message, as a CONST_STRPTR */

BOOL Locale_Initialize(void);
void Locale_Deinitialize(void);

#endif /* _LOCALE_H */
