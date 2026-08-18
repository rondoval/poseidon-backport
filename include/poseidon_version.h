/* poseidon_version.h — the $VER cookie builder shared by every component of the
 * distribution (the library, the 29 classes, Trident and the CLI tools).
 *
 * The numbers live in exactly one place: project(VERSION) in the top-level
 * CMakeLists.txt, which injects POSEIDON_VERSION / POSEIDON_REVISION / POSEIDON_DATE /
 * POSEIDON_DIST_NAME / POSEIDON_CPU globally. Nothing here hardcodes a version.
 *
 * PSD_VER("hub.class")  ->  "$VER: hub.class <ver> (<date>) Poseidon for AmigaOS 68040"
 *
 * The date stays inside the parentheses the AmigaDOS `Version` command parses; the
 * distribution name follows it, the way the classic tool cookies carry their author.
 *
 * The CPU tail is the one thing that distinguishes the release archives from each other:
 * they all carry the same version numbers (those are the library/romtag ABI), so a
 * `Version LIBS:poseidon.library` on the running machine would otherwise not say which
 * of the 68020/68040/68060 builds is installed.
 *
 * _PSD_STR is the two-level "stringify a macro's value" idiom: # does not expand its
 * operand, so _PSD_STR(POSEIDON_VERSION) expands POSEIDON_VERSION (e.g. 6) before
 * _PSD_STR2 stringifies it to "6" (vs _PSD_STR2(POSEIDON_VERSION) -> "POSEIDON_VERSION").
 */
#ifndef POSEIDON_VERSION_H
#define POSEIDON_VERSION_H

#ifndef POSEIDON_VERSION
#error "POSEIDON_VERSION/REVISION/DATE/DIST_NAME/CPU come from the top-level CMakeLists.txt"
#endif

#define _PSD_STR2(x) #x
#define _PSD_STR(x)  _PSD_STR2(x)

#define PSD_VER(name) \
    "$VER: " name " " _PSD_STR(POSEIDON_VERSION) "." _PSD_STR(POSEIDON_REVISION) \
    " (" POSEIDON_DATE ") " POSEIDON_DIST_NAME " " POSEIDON_CPU

/* Short display form for window titles and the like: PSD_NAME_VER("Trident") ->
 * "Trident <ver>". Use this instead of putting a version in a locale catalog — a
 * translated version number can only ever drift. */
#define PSD_NAME_VER(name) \
    name " " _PSD_STR(POSEIDON_VERSION) "." _PSD_STR(POSEIDON_REVISION)

#endif /* POSEIDON_VERSION_H */
