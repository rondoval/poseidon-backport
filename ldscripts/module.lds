/* SPDX-License-Identifier: MPL-2.0 OR GPL-2.0+
 *
 * module.lds - layout contract for a freestanding AmigaOS module (.library / .class).
 *
 * Derived from the stock `amiga` emulation script (m68k-amigaos-ld -m amiga --verbose),
 * which is what these targets linked with before.  Two deliberate omissions:
 *
 *   SEARCH_DIR   the driver supplies -L; hardcoding a prefix would break the container
 *                (/opt/amiga) vs a native install (/opt/m68k-amigaos).
 *   .debug_*     every module here links with -s, so DWARF is stripped anyway.  Leaving it
 *                out also keeps this script off binutils' release treadmill: the DWARF block
 *                was rewritten wholesale between 2.39 and 2.46, while everything below is
 *                byte-identical across both.  A debug build that wants DWARF can drop -Wl,-T.
 */
OUTPUT_FORMAT("amiga")
OUTPUT_ARCH(m68k)

/* HUNK has no entry-point field: LoadSeg() starts at offset 0 of the first hunk, so the
 * do-not-execute stub being FIRST is what makes running a module as a program return -1.
 * ENTRY() only names the symbol for ld; the KEEP below is what places it.  Note the stub
 * also needs __attribute__((used)) in C -- ENTRY is not an LTO root, and without `used`
 * the plugin discards the whole module and still emits a valid (empty) HUNK header. */
ENTRY(_doNotExecute)

SECTIONS
{
  PROVIDE(___machtype = 0x0);
  . = 0x0;
  .text :
  {
    __stext = .;
    KEEP(*(.text.entry))        /* moveq #-1,d0; rts -- must be at offset 0 */
    KEEP(*(.text.modhdr))       /* the header the OS reads right after it */
    *(.text)
    *(.text.main)
    *(.text*)
    *(_*)
    *(.rodata*)
    *(.data.rel.ro*)
    *(.gnu.linkonce.t.*)
    *(.gnu.linkonce.r.*)
    *(.gcc_except_table*)
    *(SORT_BY_NAME(.list___EH_FRAME*))
    *(SORT_BY_NAME(.list_*))
    *(.end_of_lists)
    ___datadata_relocs = .;
    /* RT_ENDSKIP.  Defined here rather than by a trailing end.c TU, so it bounds the whole
     * module instead of just the objects that happened to be listed before the libraries. */
    _endOfCode = .;
    __etext = .;
  }
  ___text_size = SIZEOF(.text);
  . = ALIGN(0x0);
  .data :
  {
    __sdata = .;
    *(.data)
    *(SORT_BY_NAME(.data.*))
    CONSTRUCTORS
    *(SORT_BY_NAME(.dlist___EH_FRAME_OBJECT*))
    *(SORT_BY_NAME(.dlist_*))
    *(.end_of_dlists)
    *(.data.__EH_FRAME_OBJECT__*)
    *(.gnu.linkonce.d.*)
    __edata = .;
  }
  ___data_size = SIZEOF(.data);
  .bss :
  {
    __bss_start = .;
    *(.bss)
    *(.bss.*)
    *(COMMON)
    __end = .;
  }
  ___bss_size = SIZEOF(.bss);
  .datachip :
  {
    *(.datachip)
  }
  .bsschip :
  {
    *(.bsschip)
  }

  /* A ROM module is mapped read-only, so a writable static is a cold-boot crash rather
   * than a bug you can print.  Fail the link instead.  Targets that legitimately carry
   * writable data (camdusbmidi's CAMD driver blob, which camd.library LoadSeg's) opt out with
   * psd_module_layout(<t> WRITABLE), which defines __psd_writable_ok. */
  /* PROVIDE, not DEFINED(): ld's DEFINED() does not see a --defsym symbol, but PROVIDE
   * leaves an existing one alone, so this reads 1 exactly when the target opted out. */
  PROVIDE(__psd_writable_ok = 0);
  ASSERT(__psd_writable_ok != 0 || SIZEOF(.data) == 0,
    "module has a non-empty .data section - not ROM-able. Move the static into the device/library base, or const-ify it.")
  ASSERT(__psd_writable_ok != 0 || SIZEOF(.bss) == 0,
    "module has a non-empty .bss section - not ROM-able. Move the static into the device/library base, or const-ify it.")
}
