# Bootloader (`boot/`)

The bootloader is the first code executed after the BIOS/bootloader loads MentOS.

## Contents

- **`src/`** - Bootloader source code
  - `boot.S` - Assembly entry point, CPU setup (GDT, paging)
  - `boot.c` - Bootloader main logic, kernel loading
  - `multiboot.c` - Multiboot specification handling

- **`inc/`** - Bootloader headers

- **`linker/`** - Linker scripts
  - `boot.lds` - Links the bootloader itself
  - `kernel.lds` - Links the kernel binary

## What It Does

1. **Initialization** - Sets up GDT (Global Descriptor Table), enables protected mode
2. **Multiboot Info** - Parses multiboot information from the bootloader
3. **Kernel Load** - Loads the kernel binary (which is embedded in bootloader.bin)
4. **Jump to Kernel** - Transfers control to `kmain()` in the kernel

## Output

**`build/mentos/bootloader.bin`** - The final bootable binary

This contains:

- Bootloader code
- Embedded kernel binary (as data)

## Build Process

```text
boot/src/*.c + boot/src/*.S
         ↓
   bootloader library
         ↓
kernel binary (kernel.bin)
         ↓
kernel.bin as object (kernel.bin.o)
         ↓
Link bootloader + kernel.bin.o
         ↓
bootloader.bin ✓
```

## Multiboot

MentOS uses the **Multiboot specification** to allow loading via GRUB. This enables:

- Flexible bootloaders (QEMU, physical hardware with GRUB, etc.)
- Memory map information from bootloader
- Module support

See `multiboot.c` for multiboot handling.

## Key Functions

- `boot_main()` - Main bootloader function
- `multiboot_read_info()` - Parse multiboot data
- `setup_gdt()`, `setup_idt()` - CPU initialization
- `setup_paging()` - Enable virtual memory

## Debugging

- Add `printk()` calls (kernel output function)
- Use GDB: `make qemu-gdb` then set breakpoints in boot code
- Check `build/mentos/bootloader.bin` with `objdump -d`

## Related

- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
