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

# Distribution version = poseidon.library's own $VER (so the zip name tracks the stack version,
# 5.3 at time of writing — not Trident's stale 4.4). Override with -DPOSEIDON_PKG_VERSION=...
file(STRINGS ${CMAKE_SOURCE_DIR}/poseidon.library/poseidon_main.c _ver_line
     REGEX "VER: poseidon\\.library [0-9]+\\.[0-9]+")
string(REGEX MATCH "poseidon\\.library ([0-9]+\\.[0-9]+)" _ "${_ver_line}")
set(POSEIDON_PKG_VERSION "${CMAKE_MATCH_1}" CACHE STRING "Poseidon distribution version (zip name)")
if(NOT POSEIDON_PKG_VERSION)
    set(POSEIDON_PKG_VERSION "0.0")
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

# --- CPack (ZIP) --------------------------------------------------------------
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "Poseidon")
set(CPACK_PACKAGE_VERSION "${POSEIDON_PKG_VERSION}")
set(CPACK_PACKAGE_VENDOR "Poseidon USB Stack (Chris Hodges; AROS team; native 3.2 backport)")
set(CPACK_PACKAGE_FILE_NAME "Poseidon-${POSEIDON_PKG_VERSION}")
set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY ON)   # wrap everything in the Poseidon-<ver>/ drawer
set(CPACK_ARCHIVE_FILE_NAME "${CPACK_PACKAGE_FILE_NAME}")
include(CPack)
