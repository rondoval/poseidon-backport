/* class_version.h — the class $VER cookie, shared by common.h (the class bodies)
 * and class_main.c (the romtag id-string).
 *
 * Requires CLASS_NAME from the class's CMakeLists (-D). The version numbers are the
 * distribution's (POSEIDON_VERSION/REVISION, injected globally by the top-level
 * CMakeLists) — the class fleet ships in lockstep, so no class states its own.
 */
#ifndef POSEIDON_CLASS_VERSION_H
#define POSEIDON_CLASS_VERSION_H

#include <poseidon_version.h>

#define VERSION_STRING  PSD_VER(CLASS_NAME)

#endif /* POSEIDON_CLASS_VERSION_H */
