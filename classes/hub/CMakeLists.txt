# hub.class — root + external hub driver. No GUI. The romtag priority is the shared
# PSD_CLASS_PRI default (-45) and only matters in a Kickstart ROM;
# what makes hubs bind before the device-specific classes is UCCA_Priority in hub.class.c,
# which is a different knob. See add_poseidon_class() in the top-level CMakeLists.txt.
# usbclass_headers: cross-class twin eviction calls the peer hub's usbDoMethodA (inline LVO macros).
add_poseidon_class(hub BASETYPE NepHubBase DEPS usbclass_headers)
add_dependencies(hub_class usbclass_sfd_headers)
