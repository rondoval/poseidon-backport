# hub.class — root + external hub driver. No GUI; highest class priority (47) so it binds
# before the device-specific classes. See add_poseidon_class() in the top-level CMakeLists.txt.
# usbclass_headers: cross-class twin eviction calls the peer hub's usbDoMethodA (inline LVO macros).
add_poseidon_class(hub BASETYPE NepHubBase VERSION 4 REVISION 4 PRI 47 DEPS usbclass_headers)
add_dependencies(hub_class usbclass_sfd_headers)
