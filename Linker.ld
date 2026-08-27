ENTRY(_start)

KERNEL_LOAD_ADDR = 0x100000;

SECTIONS
{
    . = KERNEL_LOAD_ADDR;

    .text ALIGN(4096) :
    {
        KEEP(*Arch/x86_64/Entry.o(.text))   /* entry point ALWAYS first */
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4096) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4096) :
    {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(4096) :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    __kernel_start = KERNEL_LOAD_ADDR;
    __kernel_end   = .;
    __kernel_size  = __kernel_end - __kernel_start;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame)
        *(.note*)
    }
}
