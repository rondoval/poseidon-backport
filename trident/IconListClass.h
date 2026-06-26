/*****************************************************************************
** This is the IconList custom class, a sub class of List.mui.
******************************************************************************/

#ifndef ICONLISTCLASS_H
#define ICONLISTCLASS_H

#define MAXMASONICONS 25

struct IconListData
{
    Object *mimainlist[MAXMASONICONS];
    Object *mimainbody[MAXMASONICONS];
};

#define TAGBASE_IconList (TAG_USER | 25<<16)

IPTR IconListDispatcher(struct IClass * cl asm("a0"), Object * obj asm("a2"), Msg msg asm("a1"));

#endif
