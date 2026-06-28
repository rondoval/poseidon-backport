# hubss.class — USB 3.0 SuperSpeed hub (AROS-native rewrite of hub.class). No GUI; hub-like.
# Struct header is hubss.class.h (NepHubSSBase/NepClassHubSS), so INCLUDE points there.
add_poseidon_class(hubss BASETYPE NepHubSSBase VERSION 4 REVISION 3 PRI 48 INCLUDE hubss.class.h)
