# PoseidonLto.cmake — link-time optimization, opted in per target.
#
# Link-time optimization, opted in per target.
#
# LTO is deliberately NOT a fourth flag level.  The three documented levels (toolchain /
# tree-wide add_compile_options / per-target -O) stay untouched: this is a CMake target
# PROPERTY, so opting a target in adds no flag anywhere and opting one out is the absence
# of a call.  CMake calls it "interprocedural optimization"; for GCC the property puts
# -flto=auto -fno-fat-lto-objects on BOTH the compile and the link line and switches the
# archive rules to gcc-ar/gcc-ranlib, which is what a static library with IR in it needs.
#
# Only the config-less INTERPROCEDURAL_OPTIMIZATION property is used: the stack never sets
# CMAKE_BUILD_TYPE, so every INTERPROCEDURAL_OPTIMIZATION_<CONFIG> form is inert.  Do not
# add one.
#
#   psd_enable_lto(<target>)                 opt <target> in
#   psd_lto_keep_real_objects(<t> <src>...)  keep these TUs real machine code
#
# The toolchain probe runs itself, once per component build, on first use.

set(POSEIDON_LTO ON CACHE BOOL "Build with GCC link-time optimization")

# check_ipo_supported() is the right probe here for a non-obvious reason: it uses
# try_compile()'s WHOLE-PROJECT form, which does not honour CMAKE_TRY_COMPILE_TARGET_TYPE
# (that governs the source-file form only), so it really does archive through gcc-ar AND
# link through the plugin -- the exact two things a binutils built --disable-plugins cannot
# do.  LANGUAGES C is explicit: CheckIPOSupported ships no ASM test project.
function(psd_lto_probe)
    if(NOT POSEIDON_LTO OR DEFINED POSEIDON_LTO_USABLE)
        return()
    endif()
    include(CheckIPOSupported)
    check_ipo_supported(RESULT _ok OUTPUT _why LANGUAGES C)
    set(POSEIDON_LTO_USABLE ${_ok} CACHE INTERNAL "LTO usable with this toolchain")
    if(NOT _ok)
        message(WARNING
            "POSEIDON_LTO=ON but this toolchain cannot do LTO - building WITHOUT it.\n"
            "  ${_why}\n"
            "  The container toolchain (scripts/docker-build.sh) can; a gcc whose binutils\n"
            "  was built --disable-plugins (the /opt/m68k-amigaos native fallback) cannot:\n"
            "  there, any LTO object inside a .a becomes an undefined reference.")
    endif()
endfunction()

function(psd_enable_lto target)
    psd_lto_probe()
    if(POSEIDON_LTO_USABLE)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
        # GCC privatises symbols during LTO and stamps visibility on them; HUNK has no such
        # concept, so the m68k-amigaos backend warns "visibility attribute not supported in
        # this configuration; ignored" once per function at ltrans -- with no codegen effect,
        # and nothing in this tree uses visibility.  Scoped to LTO'd targets so a non-LTO
        # build stays strict.
        #
        # What it costs: a MISSPELLED attribute is silenced too -- __attribute__((usedd)) or
        # ((sectoin(...))) becomes a no-op without a word.  Both are load-bearing here, which
        # is why psd_module_layout() asserts the linked module still starts with the
        # do-not-execute stub: verified that a typo'd section(".text.entry") compiles silently
        # under this flag and the layout check still fails the build.
        target_compile_options(${target} PRIVATE -Wno-attributes)
        target_link_options(${target} PRIVATE -Wno-attributes)
    endif()
endfunction()

# psd_lto_keep_real_objects(<target> <source>...)
# Compile these TUs to real machine code even under LTO.  The reason that occurs here is a
# TU whose payload is file-scope asm(): it has nothing for the IR to carry, LTO's symbol
# table cannot see what it defines, and which partition it would land in is unspecified.
function(psd_lto_keep_real_objects target)
    psd_lto_probe()
    if(POSEIDON_LTO_USABLE)
        foreach(_s IN LISTS ARGN)
            set_property(SOURCE ${_s} APPEND PROPERTY COMPILE_OPTIONS -fno-lto)
        endforeach()
    endif()
endfunction()
