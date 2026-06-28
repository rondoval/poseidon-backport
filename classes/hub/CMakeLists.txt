# hub.class — root + external hub driver. No GUI; highest class priority (47) so it binds
# before the device-specific classes. See add_poseidon_class() in the top-level CMakeLists.txt.
add_poseidon_class(hub BASETYPE NepHubBase VERSION 4 REVISION 3 PRI 47)
