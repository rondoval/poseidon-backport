# hubss.class — USB 3.0 SuperSpeed hub (AROS-native rewrite of hub.class). No GUI; hub-like.
# Struct header is hubss.class.h (NepHubSSBase/NepClassHubSS), so INCLUDE points there.
# usbclass_headers: cross-class twin eviction calls the peer hub's usbDoMethodA (inline LVO macros).
add_poseidon_class(hubss BASETYPE NepHubSSBase VERSION 4 REVISION 4 PRI 48 INCLUDE hubss.class.h DEPS usbclass_headers)
add_dependencies(hubss_class usbclass_sfd_headers)
