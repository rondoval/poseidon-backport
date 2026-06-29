# Packaging — assemble a distributable Poseidon.zip (CPack ZIP) laid out as the drawer
# the Commodore Installer script copies from. Centralised here (cmake >= 3.13 allows
# install(TARGETS) across directories) so the component CMakeLists stay focused.
#
#   make package  ->  <build>/Poseidon-<ver>.zip
#       Poseidon-<ver>/
#         Install  Install.info            the Installer script (dist/)
#         Libs/poseidon.library
#         Classes/USB/*.class
#         C/PsdStackLoader AddUSBHardware AddUSBClasses PsdDevLister PsdErrorlog
#         Prefs/Trident  Prefs/Trident.info
#         Tools/<shellapps>                (optional group, opt-in at install)
#         Catalogs/<lang>/Trident.catalog  (added by the catalog rules below)
#         Devs/DataTypes/PSD               PSD datatype descriptor  -> DEVS:DataTypes/
#         Icons/def_PSD.info               preset-file deficon      -> ENV(ARC):SYS/

# Distribution version = poseidon.library's own version (so the archive name tracks the stack
# version, 5.3 at time of writing — not Trident's stale 4.4). Read straight from the
# Override with -DPOSEIDON_PKG_VERSION=...
file(READ ${CMAKE_SOURCE_DIR}/poseidon.library/poseidon_intern.h _intern_h)
string(REGEX MATCH "define[ \t]+LIBRARY_VERSION[ \t]+([0-9]+)"  _ "${_intern_h}")
set(_ver_major "${CMAKE_MATCH_1}")
string(REGEX MATCH "define[ \t]+LIBRARY_REVISION[ \t]+([0-9]+)" _ "${_intern_h}")
set(_ver_minor "${CMAKE_MATCH_1}")
if(_ver_major STREQUAL "" OR _ver_minor STREQUAL "")
    message(WARNING "Packaging: could not read LIBRARY_VERSION/REVISION from poseidon_intern.h; defaulting to 0.0")
    set(POSEIDON_PKG_VERSION "0.0" CACHE STRING "Poseidon distribution version (archive name)")
else()
    set(POSEIDON_PKG_VERSION "${_ver_major}.${_ver_minor}" CACHE STRING "Poseidon distribution version (archive name)")
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
# Language dir = the .ct's `## language` name. (Note: français/español carry latin-1 chars — fine on
# a latin-1 Amiga; English defaults are built into Trident so missing catalogs just fall back.)
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
endforeach()

# Niche per-gadget tools — opt-in at install time (the Installer asks); shipped under Tools/.
install(TARGETS DRadioTool PencamTool PowManTool RocketTool SonixcamTool UPSTool
        RUNTIME DESTINATION Tools)

# --- the installer ------------------------------------------------------------
# Install + Install.info land in the drawer root: double-click the icon (DefaultTool
# "Installer") or run from a Shell: `Installer Install`. Install.info is a committed
# static asset (a classic project icon) — see dist/icons/ to regenerate it.
install(FILES ${CMAKE_SOURCE_DIR}/dist/Install
              ${CMAKE_SOURCE_DIR}/dist/Install.info
        DESTINATION .)

# --- the .lha distribution (`make package`) -----------------------------------
# Stage the install() layout above into a Poseidon-<ver>/ drawer, then `lha a` its contents
# so the drawer sits at the archive root.
#
#   cmake --build build               # build everything first
#   cmake --build build --target package   # -> build/Poseidon-<ver>.lha
#
# (Run a full build before `package`: the target stages whatever is currently built.)
find_program(LHA_EXECUTABLE NAMES lha)
if(NOT LHA_EXECUTABLE)
    message(WARNING "lha not found on PATH — the 'package' target will fail. "
                    "Install lha (the toolchain build image ships it) to build the distribution.")
endif()

set(LHA_FILENAME_ARGS "--system-kanji-code=utf8;--archive-kanji-code=latin1"
    CACHE STRING "lha-ac filename charset args (UTF-8 host -> latin-1 archive)")

set(_pkg_root    "${CMAKE_BINARY_DIR}/package")
set(_pkg_stage   "${_pkg_root}/Poseidon-${POSEIDON_PKG_VERSION}")
set(_pkg_archive "${CMAKE_BINARY_DIR}/Poseidon-${POSEIDON_PKG_VERSION}.lha")

add_custom_target(package
    # Re-stage cleanly from the install() rules into the versioned drawer.
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${_pkg_stage}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${_pkg_stage}"
    COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}" --prefix "${_pkg_stage}"
    # Archive the drawer (chdir into package/ so the path inside the .lha is Poseidon-<ver>/...).
    COMMAND ${CMAKE_COMMAND} -E rm -f "${_pkg_archive}"
    COMMAND ${CMAKE_COMMAND} -E chdir "${_pkg_root}"
            "${LHA_EXECUTABLE}" a ${LHA_FILENAME_ARGS} "${_pkg_archive}" "Poseidon-${POSEIDON_PKG_VERSION}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
    COMMENT "Packaging Poseidon-${POSEIDON_PKG_VERSION}.lha"
    VERBATIM)
