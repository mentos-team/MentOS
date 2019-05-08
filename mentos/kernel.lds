OUTPUT_FORMAT("elf")
OUTPUT_ARCH(i386)

ENTRY(kernel_entry)

KERNEL_PHYSICAL_ADDRESS = 0x00100000;
LOAD_MEMORY_ADDRESS     = 0x00000000;

SECTIONS
{
    . = KERNEL_PHYSICAL_ADDRESS;

    /* Put the .multiboot_header and .text section. */
    .text : AT(ADDR(.text))
    {
        _multiboot_header_start = .;
        *(.multiboot_header)
        _multiboot_header_end   = .;

        _text_start = .;
        *(.text)
        _text_end   = .;
    }

    /* Read-only data. */
    .rodata ALIGN(4K) : AT(ADDR(.rodata))
    {
        _rodata_start = .;
        *(.rodata)
        _rodata_end   = .;
    }

    /* Read-write data (initialized) */
    .data   ALIGN(4K) : AT(ADDR(.data))
    {
        _data_start = .;
        *(.data)
        _data_end   = .;
    }

    /* Read-write data (uninitialized) and stack */
    .bss    ALIGN(4K) : AT(ADDR(.bss))
    {
        _bss_start = .;
        *(.bss)
        _bss_end   = .;
    }

    /* Put a symbol end here, it tells us where all the kernel code/data ends,
       it means everything after 'end' can be used for something else. */
    end = .;
}
