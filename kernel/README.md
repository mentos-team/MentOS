# Kernel (`kernel/`)

The heart of MentOS - the core OS functionality that runs in kernel mode (ring 0).

## Quick Navigation

- **Process Management** → `src/process/`
- **Memory Management** → `src/mem/`
- **File System** → `src/fs/`
- **Device Drivers** → `src/drivers/`
- **Interrupt/Exception Handling** → `src/descriptor_tables/`
- **System Calls** → `src/system/syscall.c`
- **I/O & Output** → `src/io/`

## Directory Structure

```
kernel/
├── src/
│   ├── kernel.c              ← Kernel main entry point (kmain)
│   ├── stack.S               ← Stack initialization
│   ├── resource_tracing.c    ← Resource tracking utilities
│   ├── process/              ← Process and thread management
│   │   ├── scheduler.c       ← CPU scheduler
│   │   ├── process.c         ← Process structures
│   │   └── wait.c            ← Process wait
│   ├── mem/                  ← Memory management
│   │   ├── paging.c          ← Virtual memory
│   │   ├── mm/               ← Memory mapping
│   │   └── alloc/            ← Allocators (buddy, slab, etc)
│   ├── fs/                   ← File system
│   │   ├── ext2.c            ← EXT2 filesystem
│   │   ├── vfs.c             ← Virtual filesystem
│   │   └── pipe.c            ← Pipes
│   ├── drivers/              ← Hardware drivers
│   │   ├── keyboard/         ← Keyboard driver
│   │   ├── ata.c             ← ATA/IDE disk
│   │   └── rtc.c             ← Real-time clock
│   ├── descriptor_tables/    ← CPU tables
│   │   ├── gdt.c             ← Global Descriptor Table
│   │   ├── idt.c             ← Interrupt Descriptor Table
│   │   └── exception.c       ← Exception handlers
│   ├── system/               ← Core system functionality
│   │   ├── syscall.c         ← System calls
│   │   ├── signal.c          ← Signal handling
│   │   └── panic.c           ← Panic/error handling
│   ├── io/                   ← I/O and output
│   │   ├── video.c           ← Screen output
│   │   └── debug.c           ← Debug utilities
│   ├── ipc/                  ← Inter-process communication
│   │   ├── msg.c             ← Message queues
│   │   ├── sem.c             ← Semaphores
│   │   └── shm.c             ← Shared memory
│   ├── klib/                 ← Kernel utility library
│   │   ├── string.c          ← String utilities
│   │   ├── hashmap.c         ← Hash table
│   │   └── spinlock.c        ← Synchronization
│   ├── hardware/             ← Hardware abstractions
│   ├── elf/                  ← ELF executable loading
│   └── tests/                ← Kernel unit tests
└── inc/                      ← All kernel headers
    ├── kernel.h              ← Main kernel header
    ├── process/
    ├── mem/
    ├── fs/
    └── ...
```

## Key Entry Points

**`src/kernel.c: kmain()`** - Kernel main function
- Called by bootloader after CPU setup
- Initializes subsystems
- Starts the scheduler

**`src/process/scheduler.c`** - CPU scheduling
- Multiple scheduler algorithms (Round-Robin, Priority, CFS, EDF, etc)
- Selectable via CMake: `SCHEDULER_TYPE`

**`src/system/syscall.c`** - System call dispatcher
- Handles all userspace syscalls
- Implemented via INT 0x80

## Configuration Options

In CMake:

```bash
cmake .. -DSCHEDULER_TYPE=SCHEDULER_RR
cmake .. -DKEYMAP_TYPE=KEYMAP_IT
cmake .. -DENABLE_KERNEL_TESTS=ON
```

Available schedulers:
- `SCHEDULER_RR` - Round-robin
- `SCHEDULER_PRIORITY` - Priority-based
- `SCHEDULER_CFS` - Completely Fair Scheduler
- `SCHEDULER_EDF` - Earliest Deadline First
- `SCHEDULER_RM` - Rate Monotonic
- `SCHEDULER_AEDF` - Adaptive EDF

## How to Add Features

### Add a System Call
1. Implement function in kernel code
2. Add entry to syscall dispatcher in `src/system/syscall.c`
3. Add wrapper in `lib/inc/system/syscall_types.h`
4. Create user-facing wrapper in `lib/src/unistd/`

### Add a Device Driver
1. Create files in `src/drivers/`
2. Register with driver initialization
3. Call from `kmain()` or dynamically

### Add File System Support
1. Implement VFS operations in `src/fs/`
2. Register filesystem type
3. Handle mount in syscalls

## Debugging

- **Enable tracing**: Use CMake options like `ENABLE_KMEM_TRACE`, `ENABLE_PAGE_TRACE`
- **Debug output**: Use `printk()` function (kernel printf)
- **GDB debugging**: `make qemu-gdb` then use `gdb` from another terminal
- **Unit tests**: `make qemu-test` (runs tests as init process)

## Compilation

```bash
make                    # Build everything
make kernel.bin         # Just rebuild kernel
make qemu               # Run in QEMU
make qemu-gdb          # Run with GDB debugging
```

## Related

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Project overview
- [lib/README.md](../lib/README.md) - C Library
- [boot/README.md](../boot/README.md) - Bootloader
- [Kernel API Documentation](../doc/syscall.md)
