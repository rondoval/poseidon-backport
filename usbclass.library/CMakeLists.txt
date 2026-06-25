# usbclass is a pure ABI base — nothing opens it at runtime (psdAddClass opens each
# class library by name, and every class implements the 3 LVO functions itself). So
# there is NO usbclass.library binary; we only generate and expose its headers, which
# poseidon.library and all 30 class drivers consume.

generate_sfd_headers(usbclass USBCLASS_GEN_DIR)

add_library(usbclass_headers INTERFACE)
target_include_directories(usbclass_headers INTERFACE ${USBCLASS_GEN_DIR})
