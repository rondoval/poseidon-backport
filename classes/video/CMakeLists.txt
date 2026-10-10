# usbvideo.class - USB Video Class (UVC 1.0/1.1/1.5) cameras -> usbvideo.device. Embeds
# the device (dev.c, MakeLibrary'd in libInit). uvc_desc.c: descriptors to model (no USB
# I/O); uvc_ctrl.c: class requests and negotiation; uvc_stream.c: the stream; uvc_status.c:
# the status endpoint and its events; usbvideo.gui.c: the per-camera settings window (MUI),
# which shares the table of standard controls, tools/uvccontrols.h, with UVCTool.
# See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(usbvideo BASETYPE NepVideoBase GUI
    SOURCES uvc_desc.c uvc_ctrl.c uvc_stream.c uvc_status.c dev.c usbvideo.gui.c)
target_include_directories(usbvideo_class PRIVATE ${CMAKE_SOURCE_DIR}/tools)
