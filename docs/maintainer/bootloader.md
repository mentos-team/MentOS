# Bootloader boundary

MentOS builds two ELF images:

- `bootloader.bin` contains the Multiboot entry point, early serial diagnostics,
  ELF relocation, and temporary paging setup.
- `kernel.bin` contains the kernel and is loaded by GRUB as a separate module.

The ISO builder copies both files under `/boot`; the bootloader discovers the
module whose command line is exactly `kernel`, relocates its `PT_LOAD` segments,
and transfers control through `boot_kernel`. The bootloader is not linked
against the kernel archive. The hand-off is the only runtime boundary between
the two images.

## Include boundary

Bootloader translation units are compiled with `-nostdinc` and expose only:

- `common/inc/boot/`, which contains the Multiboot and `boot_info_t` ABI plus
  the fixed-width types needed to describe it;
- `boot/inc/`, which contains boot-local ELF, paging, CPU, console, module, and
  arithmetic helpers.

The `BOOTLOADER_BUILD` definition selects the local type declarations in
`boot/types.h`. Kernel code includes the normal freestanding libc types
instead. This keeps the shared ABI source-level common without making the
bootloader depend on `kernel/inc` or `lib/inc`.

The bootloader's `boot_console` is deliberately independent of kernel printk,
panic, scheduler, allocator, and filesystem code. An unrecoverable early-boot
condition writes a short message to COM1 and halts.

## Kernel-side Multiboot helpers

`kernel/src/multiboot.c` is the canonical implementation of memory-map and
Multiboot diagnostics used after hand-off. The bootloader uses only the data
layout in `common/inc/boot/multiboot.h` and its own module-discovery helpers;
there is no second implementation of the kernel-side helper API in `boot/`.

## Validation

Build both images after changing this boundary:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target bootloader.bin kernel.bin -j4
cmake --build build --target qemu-test -j4
```

Inspect generated dependency files when changing includes. Bootloader object
dependencies should resolve only under `common/inc` and `boot/inc`; stale files
in an existing build directory can be removed by reconfiguring or cleaning the
build tree before auditing them.
