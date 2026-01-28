# MentOS Project Architecture

Welcome to MentOS! This document explains the structure and organization of the project to help you navigate and understand how everything fits together.

## Project Overview

MentOS is a **mentoring operating system** - an educational OS designed for learning. The project is organized into several clear, independent components:

```
mentos/
├── boot/              ← Bootloader (GRUB multiboot, kernel entry)
├── kernel/            ← Kernel (core OS functionality)
├── lib/               ← C Library (standard library for kernel and userspace)
├── userspace/         ← User applications and tests
├── filesystem/        ← Root filesystem content (EXT2 image)
├── iso/               ← ISO boot files
├── doc/               ← Documentation
├── scripts/           ← Utility scripts
└── tools/             ← Build tools and CMake toolchain
```

## Component Breakdown

### 🥾 `/boot` - Bootloader

**What**: The bootloader is responsible for:
- Detecting and parsing multiboot information
- Setting up basic CPU features (GDT, IDT)
- Loading and jumping to the kernel
- Embedding the kernel binary inside itself

**Key Files**:
- `boot.c` / `boot.S` - Bootloader entry point and setup
- `multiboot.c` - Multiboot specification handling
- `linker/boot.lds` - Linker script for bootloader binary
- `linker/kernel.lds` - Linker script for kernel binary

**Output**: `build/mentos/bootloader.bin` - The final bootable binary

---

### 🐧 `/kernel` - The Kernel

**What**: The heart of MentOS, containing:
- Process and thread management (scheduler, signals)
- Memory management (paging, virtual memory, allocators)
- File system support (EXT2, pipes, procfs)
- Device drivers (keyboard, mouse, ATA, RTC, etc.)
- Interrupt and exception handling
- System calls interface
- Synchronization primitives (mutexes, spinlocks)

**Key Subdirectories**:
- `src/process/` - Process management, scheduling
- `src/mem/` - Memory management (paging, allocation)
- `src/fs/` - File system and VFS
- `src/drivers/` - Hardware drivers
- `src/descriptor_tables/` - IDT, GDT, TSS setup
- `src/system/` - Core system functionality
- `src/io/` - I/O and video output

**Key Headers**: `inc/` - Kernel API headers
**Options**: Configurable schedulers (RR, Priority, CFS, EDF, etc.), keyboard layouts

---

### 📚 `/lib` - C Standard Library

**What**: The C library provides standard functions for both:
- **Kernel** (core utilities, data structures)
- **Userspace applications** (POSIX system calls, standard C functions)

**Key Sections**:
- `src/unistd/` - POSIX system calls (fork, exec, read, write, etc.)
- `src/sys/` - System-level functions (ioctl, mman, etc.)
- `src/io/` - I/O functions (printf, scanf variants)
- `src/string.c`, `src/stdlib.c`, `src/math.c` - Standard C functions
- `src/crypt/` - Cryptographic functions (SHA256)

**Key Files**: `inc/` - All public headers

---

### 👥 `/userspace` - User Applications

**What**: Everything that runs in user-mode (ring 3):

```
userspace/
├── bin/           ← Executable programs (shell, cat, ls, etc.)
└── tests/         ← Integration and unit tests
```

**Programs** (`bin/`):
- `shell.c` - The MentOS shell
- `init.c` - System initialization
- Standard utilities: `cat.c`, `ls.c`, `cp.c`, `mkdir.c`, etc.

**Tests** (`tests/`):
- Comprehensive test suite for system functionality
- Tests for scheduling, IPC, filesystem, signals, etc.

---

### 📁 `/filesystem` - Root Filesystem

**What**: The contents of the root filesystem that gets packaged into an EXT2 image:

```
filesystem/
├── bin/          ← User applications (symlinks/copies)
├── dev/          ← Device files
├── etc/          ← System configuration
├── home/         ← User home directories
├── proc/         ← Procfs mount point
├── root/         ← Root home
└── usr/
    └── share/man ← Manual pages
```

**Purpose**: When you run `make filesystem`, this becomes `build/rootfs.img`

---

### 📖 `/doc` - Documentation

- `README.md` - General overview
- `BUILD.md` - Build instructions
- `DEVELOPMENT.md` - Development guide
- `ARCHITECTURE.md` - This file
- Various technical docs (signal handling, syscalls, etc.)

---

### 🛠️ `/tools` - Build Tools

- `toolchain-i686-elf.cmake` - CMake toolchain for i686 cross-compilation

---

## Build Process

```
┌─────────────────┐
│   Boot sources  │
│  (boot/src/)    │
└────────┬────────┘
         │
         └──────────┬────────────────────────┐
                    │                        │
              ┌─────▼─────┐          ┌──────▼──────┐
              │ bootloader │          │   Kernel    │
              │  library   │          │   library   │
              └─────┬──────┘          └──────┬──────┘
                    │                        │
                    │                  ┌─────▼──────┐
                    │                  │ kernel.bin │
                    │                  │   binary   │
                    │                  └─────┬──────┘
                    │                        │
                    │              ┌─────────▼────────┐
                    │              │ kernel.bin.o     │
                    │              │ (embedded obj)   │
                    │              └─────────┬────────┘
                    │                        │
                    └────────────┬───────────┘
                                 │
                            ┌────▼────────┐
                            │bootloader.bin│
                            │  (final)     │
                            └──────────────┘
```

The build creates:
1. **kernel library** from kernel/ sources
2. **kernel.bin** - linked kernel binary
3. **kernel.bin.o** - kernel binary as relocatable object
4. **bootloader library** from boot/ sources
5. **bootloader.bin** - final bootable binary (with embedded kernel)

## Dependency Graph

```
Userspace Programs ──┐
                     ├──► libc (C library)
Kernel Code ─────────┤
Bootloader Code ─────┘

System Calls
Userspace ◄────────────────┐
                           │
Kernel (syscall handler) ──┘
```

## How to Navigate

**I want to...**

- **Understand process scheduling**: Look in `kernel/src/process/scheduler.c`
- **Add a new system call**: Edit `kernel/src/system/syscall.c` + add to `lib/inc/system/syscall.h`
- **Create a new userspace program**: Add `.c` file to `userspace/bin/`
- **Debug the bootloader**: Check `boot/src/boot.c` and `boot.lds`
- **Modify memory management**: `kernel/src/mem/`
- **Change filesystem**: `kernel/src/fs/`
- **Add driver support**: `kernel/src/drivers/`

## Quick Reference

| What | Where |
|------|-------|
| Boot code | `boot/src/` |
| Kernel | `kernel/src/` |
| Kernel headers | `kernel/inc/` |
| C Library | `lib/src/` |
| C Library headers | `lib/inc/` |
| User programs | `userspace/bin/` |
| Tests | `userspace/tests/` |
| Build output | `build/` |

## Getting Started

1. **Build the project**: `make` or `cmake --build build`
2. **Run in QEMU**: `make qemu`
3. **Debug with GDB**: `make qemu-gdb` in one terminal, `gdb -x gdb.run` in another
4. **Run tests**: `make qemu-test`

See [BUILD.md](../doc/BUILD.md) for detailed instructions.

---

**Happy hacking!** 🎉
