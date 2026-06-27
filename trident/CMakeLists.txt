# Trident — the Poseidon MUI control panel. A CRT *program* (not a library): default
# CRT link, client base-var (POSEIDON_BASE_NAME=ps, no __NOLIBBASE__), plain global
# MUIMasterBase. 4 MUI custom classes + embedded logo/icon data + localized catalogs.

find_program(FLEXCAT_EXECUTABLE NAMES flexcat HINTS $ENV{HOME}/.local/bin /opt/m68k-amigaos/bin REQUIRED)

# Generate strings.h (the CatCompArray + MSG_* enum) from trident.cd via flexcat +
# the AROS catcomp template, into <bindir>/generated/.
set(_strings ${CMAKE_CURRENT_BINARY_DIR}/generated/strings.h)
add_custom_command(OUTPUT ${_strings}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/generated
    COMMAND ${FLEXCAT_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/catalogs/trident.cd
            ${_strings}=${CMAKE_CURRENT_SOURCE_DIR}/catalogs/C_h_aros.sd
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/catalogs/trident.cd
            ${CMAKE_CURRENT_SOURCE_DIR}/catalogs/C_h_aros.sd
    VERBATIM)
add_custom_target(trident_strings DEPENDS ${_strings})

# Localized .catalog files (runtime assets — English defaults work without them).
# flexcat returns RETURN_WARN(5) on translation gaps but still writes the catalog,
# so tolerate a non-zero exit. Built into <bindir>/catalogs/ for manual install to
# LOCALE:Catalogs/<language>/System/Prefs/Trident.catalog.
set(TRIDENT_LANGS czech french italian polish russian spanish)
set(_catalogs)
foreach(lang ${TRIDENT_LANGS})
    set(_cat ${CMAKE_CURRENT_BINARY_DIR}/catalogs/${lang}.catalog)
    add_custom_command(OUTPUT ${_cat}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/catalogs
        COMMAND sh -c "'${FLEXCAT_EXECUTABLE}' '${CMAKE_CURRENT_SOURCE_DIR}/catalogs/trident.cd' '${CMAKE_CURRENT_SOURCE_DIR}/catalogs/${lang}.ct' CATALOG '${_cat}' || true"
        DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/catalogs/trident.cd
                ${CMAKE_CURRENT_SOURCE_DIR}/catalogs/${lang}.ct
        VERBATIM)
    list(APPEND _catalogs ${_cat})
endforeach()
add_custom_target(trident_catalogs ALL DEPENDS ${_catalogs})

file(GLOB MASON_ICONS CONFIGURE_DEPENDS MasonIcons/*.c)

add_executable(Trident
    Trident.c
    ActionClass.c
    CfgListClass.c
    DevWinClass.c
    IconListClass.c
    locale.c
    neptune8logo.c
    ${MASON_ICONS})

add_dependencies(Trident poseidon_sfd_headers usbclass_sfd_headers trident_strings)

target_compile_options(Trident PRIVATE
    -O2 -m68040 -mhard-float -fomit-frame-pointer -Wno-int-conversion
    -DPOSEIDON_BASE_NAME=ps
    -include ${CMAKE_SOURCE_DIR}/include/aros_compat.h)

target_include_directories(Trident PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/generated)   # the flexcat-generated strings.h

target_link_libraries(Trident PRIVATE
    poseidon_headers
    usbclass_headers
    mui_headers
    -lamiga)        # DoMethod / DoSuperMethodA / MUI vararg macros

psd_debug_finalize(Trident)   # links -ldebug only for the serial backend (Trident has no log sites)
