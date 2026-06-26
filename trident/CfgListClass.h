/*****************************************************************************
** This is the CfgList custom class, a sub class of List.mui.
******************************************************************************/
#ifndef CFGLISTCLASS_H
#define CFGLISTCLASS_H

struct CfgListData
{
    BOOL cl_Dragging;
};

#define TAGBASE_CfgList (TAG_USER | 3242<<16)

IPTR CfgListDispatcher(struct IClass * cl asm("a0"), Object * obj asm("a2"), Msg msg asm("a1"));

#endif 
