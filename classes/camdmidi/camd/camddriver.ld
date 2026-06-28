/* camddriver.ld — link script for the embedded CAMD MIDI driver (camd/poseidonusb.c).
 *
 * A CAMD driver's loaded image must begin with
 *     offset 0:  moveq #-1,d0 ; rts     (4-byte do-not-execute stub, section .camdhdr)
 *     offset 4:  struct MidiDeviceData  (Magic == 'MDEV', section .camdmagic)
 * which is what camd.library reads after LoadSeg(). C section attributes choose *which*
 * section a symbol lands in, but only the linker can order the output sections — hence this
 * script: KEEP .camdhdr then .camdmagic first in .text.
 *
 * This is a minimal replacement for amiga.xe: verified to produce a BYTE-IDENTICAL driver to
 * the full stock script for this freestanding TU (no constructors / EH / chip sections), so
 * none of amiga.xe's extra machinery is needed here. gen_camddriver.py asserts stub+'MDEV' at
 * offset 4, so any future drift fails the build. */
OUTPUT_FORMAT("amiga")
OUTPUT_ARCH(m68k)
SECTIONS
{
  . = 0x0;
  .text : {
    KEEP(*(.camdhdr))         /* moveq #-1,d0; rts  -> offset 0 */
    KEEP(*(.camdmagic))       /* struct MidiDeviceData -> offset 4 */
    *(.text*)
    *(.rodata*)
    *(_*)
    *(.data.rel.ro*)
  }
  .data : { *(.data*) }
  .bss  : { *(.bss*) *(COMMON) }
}
