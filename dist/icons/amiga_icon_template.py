"""Write a minimal, self-authored classic DiskObject (.info) to use as an
icontool import template — no third-party icon bytes embedded.
Header layout per icon.library.

Used by make_icons.py.
"""
import struct

# Workbench icon type codes (DiskObject.do_Type)
TYPE_CODE = {
    "DISK": 1, "DRAWER": 2, "TOOL": 3, "PROJECT": 4,
    "GARBAGE": 5, "DEVICE": 6, "KICK": 7, "APPICON": 8,
}


def write_template(path, itype="TOOL", stack=4096):
    """itype: a TYPE_CODE name (e.g. 'TOOL') or an int code. stack: do_StackSize."""
    code = TYPE_CODE.get(itype.upper(), 3) if isinstance(itype, str) else int(itype)
    b = bytearray()
    b += struct.pack(">H", 0xE310)       # Magic (WB_DISKMAGIC)
    b += struct.pack(">H", 1)            # Version
    b += struct.pack(">I", 0)            # Gadget.NextGadget
    b += struct.pack(">HH", 0, 0)        # Left,Top
    b += struct.pack(">HH", 8, 8)        # Width,Height (bumped on import)
    b += struct.pack(">H", 0x0005)       # Flags  GADGHBOX|GADGIMAGE
    b += struct.pack(">H", 0x0003)       # Activation RELVERIFY|GADGIMMEDIATE
    b += struct.pack(">H", 1)            # GadgetType
    b += struct.pack(">I", 0x000000FF)   # GadgetRender (fixed up by icon.library)
    b += struct.pack(">I", 0)            # SelectRender = 0 -> single image
    b += struct.pack(">I", 0)            # GadgetText
    b += struct.pack(">I", 0)            # MutualExclude
    b += struct.pack(">I", 0)            # SpecialInfo
    b += struct.pack(">H", 0)            # GadgetID
    b += struct.pack(">I", 1)            # UserData (low byte = revision 1)
    b += struct.pack(">B", code)         # Type
    b += struct.pack(">B", 0)            # Padding
    b += struct.pack(">I", 0)            # DefaultTool presence (none yet)
    b += struct.pack(">I", 0)            # ToolTypes presence
    b += struct.pack(">i", -2147483648)  # CurrentX = NO_ICON_POSITION
    b += struct.pack(">i", -2147483648)  # CurrentY
    b += struct.pack(">I", 0)            # DrawerData
    b += struct.pack(">I", 0)            # ToolWindow
    b += struct.pack(">I", int(stack))   # StackSize
    assert len(b) == 78, len(b)
    b += struct.pack(">HH", 0, 0)        # Image Left,Top
    b += struct.pack(">HH", 8, 8)        # Width,Height
    b += struct.pack(">H", 1)            # Depth
    b += struct.pack(">I", 0x12345678)   # ImageData placeholder
    b += struct.pack(">bb", 1, 0)        # PlanePick,PlaneOnOff
    b += struct.pack(">I", 0)            # NextImage
    b += bytes(((8 + 15) // 16) * 2 * 8 * 1)   # planar bitmap (16 zero bytes)
    with open(path, "wb") as f:
        f.write(bytes(b))
