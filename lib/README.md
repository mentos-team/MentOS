# C Library (`lib/`)

The standard C library for MentOS - provides C standard library functions and system call wrappers.

## What It Provides

The library serves two audiences:

1. **Kernel** - Core utilities, data structures, math functions
2. **Userspace** - Standard C library and POSIX system call wrappers

## Contents

```
lib/
├── src/
│   ├── stdlib.c, string.c, math.c  ← Standard C functions
│   ├── stdio.c, scanf.c            ← I/O functions
│   ├── ctype.c, time.c             ← Character, time utilities
│   ├── unistd/                     ← POSIX system calls
│   │   ├── fork.c, exec.c          ← Process management
│   │   ├── read.c, write.c         ← I/O syscalls
│   │   ├── open.c, close.c         ← File operations
│   │   ├── getpid.c, kill.c        ← Process info/signals
│   │   └── ...
│   ├── sys/                        ← System-level calls
│   │   ├── mman.c                  ← Memory mapping
│   │   ├── ioctl.c                 ← Device control
│   │   └── ipc.c                   ← IPC operations
│   ├── pwd.c, grp.c, shadow.c      ← User/group management
│   ├── sched.c, signal.c           ← Scheduling, signals
│   ├── list.c, hashmap.c, ndtree.c ← Data structures
│   ├── crypt/sha256.c              ← Cryptography
│   ├── crt0.S                      ← C runtime startup
│   ├── libc_start.c                ← libc initialization
│   └── ...
└── inc/                            ← All public headers
    ├── stdlib.h, string.h, math.h  ← Standard headers
    ├── unistd.h, fcntl.h           ← POSIX headers
    ├── sys/                        ← sys/ headers
    └── ...
```

## Standard Functions

### String & Memory
- `strcpy()`, `strlen()`, `strcmp()`, `memcpy()`, `memset()`, etc.

### I/O
- `printf()`, `sprintf()`, `scanf()`, `fprintf()` (kernel-compatible versions)

### Data Structures
- `list.c` - Linked lists
- `hashmap.c` - Hash tables
- `ndtree.c` - N-dimensional trees

### Math
- `sqrt()`, `sin()`, `cos()`, `pow()`, etc.

## System Call Wrappers

The library wraps kernel system calls for easy userspace access:

```c
// User code (userspace)
#include <unistd.h>
pid_t child = fork();  // Wrapper in lib/src/unistd/fork.c
                       // Calls int 0x80 to kernel

// Kernel handles syscall
// Returns result to userspace
```

### Common Syscalls

- **Process**: `fork()`, `exec()`, `exit()`, `wait()`, `waitpid()`
- **Signals**: `signal()`, `kill()`, `raise()`
- **I/O**: `open()`, `close()`, `read()`, `write()`, `lseek()`
- **Files**: `stat()`, `mkdir()`, `rmdir()`, `unlink()`, `chdir()`
- **Users**: `getuid()`, `getgid()`, `setuid()`, `setgid()`
- **IPC**: `msgget()`, `semget()`, `shmget()`

## Implementation Pattern

### In libc:

```c
// lib/src/unistd/fork.c
#include <unistd.h>
#include <sys/syscall.h>

pid_t fork(void) {
    return _syscall0(SYS_fork);
}
```

The `_syscall0()`, `_syscall1()`, etc. macros issue the INT 0x80 interrupt, which:
1. Switches to kernel mode
2. Kernel dispatcher reads syscall number and arguments
3. Kernel executes syscall
4. Returns to userspace

### In kernel:

```c
// kernel/src/system/syscall.c
case SYS_fork:
    return sys_fork(ctx);
```

## Compilation

The library compiles into:
- **`build/libc`** - Static library (libc.a or similar)
- Used by both kernel and userspace programs

## Linking

- **Kernel**: Linked with kernel code to provide utilities
- **Userspace programs**: Linked with full libc to provide C standard library + syscall wrappers

## Adding System Calls

1. **Define syscall number** in `kernel/inc/system/syscall_types.h`
2. **Implement in kernel** in `kernel/src/system/`
3. **Add dispatcher entry** in `kernel/src/system/syscall.c`
4. **Create wrapper** in `lib/src/unistd/` (or appropriate location)
5. **Add header** in `lib/inc/`

## Features

### Data Structures
- Linked lists: `list.h`
- Hash maps: `hashmap.h`
- N-D trees: `ndtree.h`
- Ring buffers: `ring_buffer.h`

### String Functions
- Format strings: `vsprintf()`, `vscanf()`
- Memory operations: `strcpy()`, `memcpy()`, `strlen()`, etc.

### Math Library
- Trigonometric: `sin()`, `cos()`, `tan()`
- Power: `pow()`, `sqrt()`, `exp()`, `log()`
- Utilities: `abs()`, `min()`, `max()`

## Debugging

- Check `lib/src/` for implementation details
- Wrapper functions should have minimal logic (just syscall invocation)
- Use `strace`-like debugging to trace syscalls

## Related

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Project overview
- [kernel/README.md](../kernel/README.md) - Kernel
- [syscall.md](../doc/syscall.md) - System call documentation
