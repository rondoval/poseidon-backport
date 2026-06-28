# hid.class — full USB HID (keyboard/mouse/joystick/tablet). Feeds input.device directly (the native
# Hodges path; the AROS OOP/HIDD blocks are #if __AROS__ and excluded). Also patches lowlevel.library
# ReadJoyPort/SetJoyPortAttrsA for USB gamepads. Two MUI GUIs (config + per-binding control) +
# numtostr.c string tables. Per-instance MUI base (NepClassHid/nch_MUIBase).
add_poseidon_class(hid BASETYPE NepHidBase VERSION 4 REVISION 5 PRI 29 GUI
    SOURCES hid.gui.c hidctrl.gui.c numtostr.c DEFINES HAS_LIBOPEN HAS_LIBCLOSE)
