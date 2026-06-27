/* poseidon_debug_serial_glue.c — glue for the serial debug backend (debug.lib).
 *
 * Added to every target that links libdebug.a by psd_debug_finalize() (serial backend
 * only). debug.lib ships as a single object; pulling in KPutChar can also drag KGetNum,
 * which references the compiler helper __divsi3. Provide it weak so libc's strong copy
 * wins for hosted programs, while it is the sole definition for freestanding (-nostdlib)
 * targets. As a real object it also satisfies the reference regardless of the trailing
 * position of -ldebug on the link line (after the components' --start-group).
 */
__asm__(
    "    .text\n"
    "    .weak    ___divsi3\n"
    "___divsi3:\n"
    "    divs.l   %d1,%d0\n"
    "    rts\n"
);
