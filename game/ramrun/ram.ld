/* SRAM layout for the tethered test: loaded at the same address the ROM uses for its loaders. */
ENTRY(entry)

SECTIONS
{
    . = 0x1c02000;
    .text : {
        KEEP(*(.text.entry))
        *(.text*)
    }
    .rodata : ALIGN(4) { *(.rodata*) }
    .data : ALIGN(4) { *(.data*) *(.sdata*) }
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start = .;
        *(.bss*) *(.sbss*) *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    }
    /DISCARD/ : { *(.comment) *(.note*) }
}
