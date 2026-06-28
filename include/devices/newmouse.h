#ifndef DEVICES_NEWMOUSE_H
#define DEVICES_NEWMOUSE_H
/*
 * devices/newmouse.h — the "NewMouse" standard for wheeled / extra-button mice.
 *
 * NewMouse is a community standard (Alessandro Zumo, 1999; Aminet util/mouse/newmouse.lha),
 * NOT part of the official AmigaOS API — so none of these are in the NDK. Wheel / 4th-button
 * events are delivered both as IECLASS_NEWMOUSE and as IECLASS_RAWKEY with the codes below.
 */

#ifndef IECLASS_NEWMOUSE
#define IECLASS_NEWMOUSE        0x16   /* IECLASS_MAX + 1 as of V40 */
#endif

#define RAWKEY_NM_WHEEL_UP      0x7A
#define RAWKEY_NM_WHEEL_DOWN    0x7B
#define RAWKEY_NM_WHEEL_LEFT    0x7C
#define RAWKEY_NM_WHEEL_RIGHT   0x7D
#define RAWKEY_NM_BUTTON_FOURTH 0x7E

#endif /* DEVICES_NEWMOUSE_H */
