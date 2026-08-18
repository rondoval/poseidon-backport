# Packaging — assemble a distributable Poseidon archive (LhA, the Amiga-native format)
# laid out as the drawer the Commodore Installer script copies from. Centralised here
# (cmake >= 3.13 allows install(TARGETS) across directories) so the component
# CMakeLists stay focused.
#
#   make package  ->  <build>/Poseidon-<ver>-<cpu>[-<backend>].lha
#       Poseidon-<ver>-<cpu>[-<backend>]/
#         Install  Install.info            the Installer script (dist/)
#         Libs/poseidon.library
#         Classes/USB/*.class
#         C/PsdStackLoader AddUSBHardware AddUSBClasses PsdDevLister PsdErrorlog
#         Prefs/Trident  Prefs/Trident.info
#         WBStartup/USBEject  WBStartup/USBEject.info  (safe-eject Workbench menu, opt-in)
#         Tools/<shellapps>                (optional group, opt-in at install)
#         Catalogs/<lang>/System/Prefs/Trident.catalog  Catalogs/<lang>/USBEject.catalog
#         Devs/DataTypes/PSD               PSD datatype descriptor  -> DEVS:DataTypes/
#         Icons/def_PSD.info               preset-file deficon      -> ENV(ARC):SYS/

# Distribution version = the project version (top-level CMakeLists), which is also what every
# component reports in its $VER — so the archive name and the fleet can never disagree.
# Override with -DPOSEIDON_PKG_VERSION=... (that lands in the cache and still wins here).
# Deliberately NOT a cache variable of its own: a cached copy initialises once and then
# sticks, so bumping project(VERSION) in an existing build dir would quietly package the
# new fleet inside an archive named after the old version.
if(NOT POSEIDON_PKG_VERSION)
    set(POSEIDON_PKG_VERSION "${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}")
endif()

# --- the built artifacts, into the distribution drawer layout ------------------
install(TARGETS poseidon_library RUNTIME DESTINATION Libs)

# Every USB class — the full fleet, gathered from the global list add_poseidon_class()
# appends to (CMakeLists.txt). No hand-maintained roster: add a class, it ships.
get_property(_poseidon_classes GLOBAL PROPERTY POSEIDON_CLASS_TARGETS)
install(TARGETS ${_poseidon_classes} RUNTIME DESTINATION Classes/USB)

install(TARGETS PsdStackLoader AddUSBHardware AddUSBClasses PsdDevLister PsdErrorlog
        RUNTIME DESTINATION C)

install(TARGETS Trident RUNTIME DESTINATION Prefs)

# Trident's own Workbench icon (a ColorIcon — the AROS Gorilla USB-plug, GPL; see LEGAL).
# Lands next to the Trident executable; the Install script's `copyfiles Prefs (all)` carries it.
install(FILES ${CMAKE_SOURCE_DIR}/dist/Trident.info DESTINATION Prefs)

# Poseidon prefs-file datatype + default icon. DefIcons (a standard 3.2 WBStartup component)
# identifies FORM..PSLC config files via the PSD datatype and paints def_PSD.info on the ones
# that lack their own icon. Descriptor -> DEVS:DataTypes/PSD ; def_PSD.info -> ENV(ARC):SYS/.
install(FILES ${CMAKE_SOURCE_DIR}/dist/datatypes/PSD DESTINATION Devs/DataTypes)
install(FILES ${CMAKE_SOURCE_DIR}/dist/def_PSD.info DESTINATION Icons)

# Default USB attach/detach sounds → SYS:Prefs/Presets/Poseidon/ (poseidon.library.c:198-199 points
# its default Insert/Remove sound files here). Capitalised to match the code's path (FFS is
# case-insensitive, but keep it tidy).
install(FILES ${CMAKE_SOURCE_DIR}/presets/Poseidon/connect.iff
        DESTINATION Prefs/Presets/Poseidon RENAME Connect.iff)
install(FILES ${CMAKE_SOURCE_DIR}/presets/Poseidon/disconnect.iff
        DESTINATION Prefs/Presets/Poseidon RENAME Disconnect.iff)

# Trident catalogs (built by the trident_catalogs target) → Catalogs/<locale-language>/System/Prefs/
# so the Install script's `copyfiles Catalogs → LOCALE:Catalogs` lands them where OpenCatalog looks.
# Language dir = the .ct's `## language` name, which is what locale.library looks up — and on an
# Amiga that name is latin-1 (français = 0xE7, español = 0xF1), matching the .ct `## language`
# lines. This file is UTF-8, so the two names below are UTF-8 here and the staged directories are
# too; the single point of correctness is the lha charset transform (LHA_FILENAME_ARGS, below),
# which stores latin-1 names in the archive. `cmake --install` has no such transform and stages raw
# UTF-8 — fine for the CI artifacts that use it, but never a release path. English defaults are
# built into Trident, so a mis-named or missing catalog just falls back.
set(_cat_files   czech    french     italian  polish  russian  spanish)
set(_cat_langs   czech   "français"  italiano polski  russian "español")
list(LENGTH _cat_files _n)
math(EXPR _n "${_n} - 1")
foreach(i RANGE ${_n})
    list(GET _cat_files ${i} _f)
    list(GET _cat_langs ${i} _l)
    install(FILES ${CMAKE_BINARY_DIR}/trident/catalogs/${_f}.catalog
            DESTINATION "Catalogs/${_l}/System/Prefs"
            RENAME Trident.catalog)
    # USBEject lives in SYS:WBStartup, so its catalog goes by plain name:
    # OpenCatalog(NULL, "USBEject.catalog") -> LOCALE:Catalogs/<lang>/USBEject.catalog
    install(FILES ${CMAKE_BINARY_DIR}/usbeject/catalogs/${_f}.catalog
            DESTINATION "Catalogs/${_l}"
            RENAME USBEject.catalog)
endforeach()

# Niche per-gadget tools — opt-in at install time (the Installer asks); shipped under Tools/.
install(TARGETS DRadioTool PencamTool PowManTool RocketTool SonixcamTool UPSTool
        RUNTIME DESTINATION Tools)

# USBEject safe-eject daemon → SYS:WBStartup (the Installer asks; icon carries DONOTWAIT).
install(TARGETS USBEject RUNTIME DESTINATION WBStartup)
install(FILES ${CMAKE_SOURCE_DIR}/dist/USBEject.info DESTINATION WBStartup)

# --- the installer ------------------------------------------------------------
# Install + Install.info land in the drawer root: double-click the icon (DefaultTool
# "Installer") or run from a Shell: `Installer Install`. Install.info is a committed
# static asset (a classic project icon) — see dist/icons/ to regenerate it.
install(FILES ${CMAKE_SOURCE_DIR}/dist/Install
              ${CMAKE_SOURCE_DIR}/dist/Install.info
        DESTINATION .)

# --- generated ReadMe (self-describes the build variant, per archive) ---------
# Two variant axes, both stamped into the ReadMe so an unpacked drawer always says what
# it is: the CPU (68020/68040/68060) and the debug backend. off -> production (no debug);
# serial -> serial @ 9600; any other backend -> the Emu68/PiStorm debug console. Stamped
# via configure_file and dropped in the drawer root next to Install. Mirrors
# emu68-driver-stack's @DEBUG_BACKEND@ ReadMe.
#
# No variant needs an FPU: the stack itself has no floating point at all, and the only
# code that does — the gamma table in the optional PencamTool/SonixcamTool — is soft-float
# in the 68020 build and emulated by 68040.library/68060.library on an LC part.
if(M68K_CPU STREQUAL "68020")
    set(CPU_DESCRIPTION
        "For the 68020 and 68030. No FPU required. This build also runs on a\n  68040 or 68060, but the matching archive is tuned for those.")
elseif(M68K_CPU STREQUAL "68040")
    set(CPU_DESCRIPTION
        "For the 68040, including PiStorm/Emu68. No FPU required.")
elseif(M68K_CPU STREQUAL "68060")
    set(CPU_DESCRIPTION
        "For the 68060. No FPU required.")
else()
    set(CPU_DESCRIPTION "Built for the ${M68K_CPU}.")
endif()

set(DEBUG_BACKEND "${POSEIDON_DEBUG_BACKEND}")
if(POSEIDON_DEBUG_BACKEND STREQUAL "off")
    set(DEBUG_BACKEND_DESCRIPTION
        "Production build - USB stack debug output is disabled.")
elseif(POSEIDON_DEBUG_BACKEND STREQUAL "serial")
    set(DEBUG_BACKEND_DESCRIPTION
        "Debug build - debug output is sent to the Amiga serial port (9600 baud)\n  on real hardware, or can be captured/redirected on a host with Sashimi.")
else()
    set(DEBUG_BACKEND_DESCRIPTION
        "Debug build (${POSEIDON_DEBUG_BACKEND} backend) - debug output is routed\n  through the Emu68/PiStorm debug console.")
endif()
configure_file("${CMAKE_SOURCE_DIR}/dist/ReadMe.in" "${CMAKE_BINARY_DIR}/ReadMe" @ONLY)
install(FILES "${CMAKE_BINARY_DIR}/ReadMe" DESTINATION .)

# Licensing in the drawer root: LICENSE is the full AROS Public License text;
# LEGAL records the primary license + third-party attributions (lan78xx ISC, the
# mounter submodule, the GPL Trident icon). The whole stack is one license, so a
# single pair of files at the root suffices (no per-component summary needed).
install(FILES ${CMAKE_SOURCE_DIR}/LICENSE
              ${CMAKE_SOURCE_DIR}/LEGAL
        DESTINATION .)

# --- the .lha distribution (`make package`) -----------------------------------
# Stage the install() layout above into a Poseidon-<ver>/ drawer, then `lha a` its contents
# so the drawer sits at the archive root.
#
#   cmake --build build               # build everything first
#   cmake --build build --target package   # -> build/Poseidon-<ver>-<cpu>.lha
#
# (Run a full build before `package`: the target stages whatever is currently built.)
find_program(LHA_EXECUTABLE NAMES lha)
if(NOT LHA_EXECUTABLE)
    message(WARNING "lha not found on PATH — the 'package' target will fail. "
                    "Install lha (the toolchain build image ships it) to build the distribution.")
endif()

set(LHA_FILENAME_ARGS "--system-kanji-code=utf8;--archive-kanji-code=latin1"
    CACHE STRING "lha-ac filename charset args (UTF-8 host -> latin-1 archive)")

# Suffix the package by debug backend so a release can ship both variants side by side.
# 'off' is the production download and carries no suffix; 'serial' gets -serial (and any
# other backend its own name) so e.g. `make package` with serial debug -> Poseidon-<ver>-serial.lha.
if(POSEIDON_DEBUG_BACKEND STREQUAL "off")
    set(_pkg_suffix "")
elseif(POSEIDON_DEBUG_BACKEND STREQUAL "serial")
    set(_pkg_suffix "-serial")
else()
    set(_pkg_suffix "-${POSEIDON_DEBUG_BACKEND}")
endif()

# The CPU is the other variant axis: the release ships one archive per CPU. Tag derived
# from M68K_CPU (68040 -> 040) rather than a hand-kept map, and placed ahead of the
# backend suffix so the two compose: Poseidon-<ver>-060-serial.lha. The tag also rides in
# _pkg_stage below, so the drawer inside the archive carries it too — three variants can
# be unpacked side by side without colliding.
string(REGEX REPLACE "^68" "" _cpu_tag "${M68K_CPU}")

set(_pkg_name    "Poseidon-${POSEIDON_PKG_VERSION}-${_cpu_tag}${_pkg_suffix}")
set(_pkg_root    "${CMAKE_BINARY_DIR}/package")
set(_pkg_stage   "${_pkg_root}/${_pkg_name}")
set(_pkg_archive "${CMAKE_BINARY_DIR}/${_pkg_name}.lha")

add_custom_target(package
    # Re-stage cleanly from the install() rules into the versioned drawer.
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_pkg_stage}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_pkg_stage}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${_pkg_stage}"
    # Archive the drawer (chdir into package/ so the path inside the .lha is <_pkg_name>/...).
    COMMAND ${CMAKE_COMMAND} -E rm -f "${_pkg_archive}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${_pkg_root}"
            "${LHA_EXECUTABLE}" a ${LHA_FILENAME_ARGS} "${_pkg_archive}" "${_pkg_name}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Packaging ${_pkg_name}.lha"
    VERBATIM)
