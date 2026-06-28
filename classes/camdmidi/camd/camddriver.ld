/* camddriver.ld — link script for the embedded CAMD MIDI driver (camd/poseidonusb.c).
 *
 * Derived from the stock amiga.xe. The only change: KEEP the entry stub (.camdhdr) and
 * the MidiDeviceData table (.camdmagic) FIRST in .text, so the loaded image is
 *     offset 0:  moveq #-1,d0 ; rts        (4-byte do-not-execute stub)
 *     offset 4:  struct MidiDeviceData     (Magic == 'MDEV')
 * which is the layout camd.library reads after LoadSeg(). Everything else matches amiga.xe.
 */
OUTPUT_FORMAT("amiga")
OUTPUT_ARCH(m68k)
SEARCH_DIR("/opt/m68k-amigaos/m68k-amigaos/lib");
SECTIONS
{
  PROVIDE(___machtype = 0x0);
  . = 0x0;
  .text :
  {
    __stext = .;
    KEEP(*(.camdhdr))         /* moveq #-1,d0; rts  -> offset 0 */
    KEEP(*(.camdmagic))       /* struct MidiDeviceData -> offset 4 */
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
}
