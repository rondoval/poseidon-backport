/* class_version.h — the single definition of the class $VER cookie and the stringify
 * helpers behind it, shared by common.h (the class bodies) and class_main.c (the romtag
 * id-string).
 *
 * Requires CLASS_NAME / CLASS_VERSION / CLASS_REVISION from the class's CMakeLists (-D).
 * _PSD_STR is the two-level "stringify a macro's value" idiom: # does not expand its
 * operand, so _PSD_STR(CLASS_VERSION) expands CLASS_VERSION (e.g. 4) before _PSD_STR2
 * stringifies it to "4" (vs _PSD_STR2(CLASS_VERSION) -> "CLASS_VERSION").
 */
#ifndef POSEIDON_CLASS_VERSION_H
#define POSEIDON_CLASS_VERSION_H

#define _PSD_STR2(x) #x
#define _PSD_STR(x)  _PSD_STR2(x)

#define VERSION_STRING \
    "$VER: " CLASS_NAME " " _PSD_STR(CLASS_VERSION) "." _PSD_STR(CLASS_REVISION) " (Poseidon)"

#endif /* POSEIDON_CLASS_VERSION_H */
