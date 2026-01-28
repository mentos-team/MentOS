# Userspace Applications (`userspace/`)

All user-mode applications and tests that run in ring 3.

## Structure

```
userspace/
├── bin/        ← Executables (shell, utilities, etc.)
└── tests/      ← Test programs
```

## User Programs (`bin/`)

Standard utilities and system programs:

### System Programs
- **`init.c`** - Initial process (PID 1), mounts filesystem, starts shell
- **`shell.c`** - Command-line shell with pipe support, job control
- **`login.c`** - User login interface

### File Management
- `cat.c` - Display file contents
- `ls.c` - List directory contents
- `mkdir.c` - Create directories
- `rmdir.c` - Remove directories
- `cp.c` - Copy files
- `rm.c` - Remove files
- `touch.c` - Create/update files

### Utilities
- `echo.c` - Print text
- `pwd.c` - Print working directory
- `cd.c` - Change directory (built-in to shell)
- `clear.c` - Clear screen
- `stat.c` - File statistics
- `chmod.c`, `chown.c` - Permission changes
- `id.c` - User/group ID information
- `ps.c` - List processes
- `kill.c` - Send signals

### Hardware & System
- `cpuid.c` - CPU information
- `date.c` - Date and time
- `uname.c` - System information
- `uptime.c` - System uptime
- `poweroff.c` - Shutdown system

### IPC & Maintenance
- `ipcs.c`, `ipcrm.c` - IPC resource management
- `man.c` - Manual pages
- `nice.c` - Change process priority
- `edit.c` - Text editor
- `logo.c` - Display MentOS logo
- `showpid.c` - Show PID

### Testing
- `runtests.c` - Test runner

## Test Programs (`tests/`)

Comprehensive test suite (~60 tests) covering:

### Process Management
- `t_fork.c`, `t_exec.c` - Process creation
- `t_exit.c`, `t_wait*` - Process exit
- `t_kill.c` - Signal delivery
- `t_periodic*.c` - Real-time scheduling

### Memory
- `t_mem.c` - Memory allocation
- `t_write_read.c`, `t_big_write.c` - I/O

### File System
- `t_chdir.c` - Directory operations
- `t_mkdir.c`, `t_creat.c` - File creation
- `t_fhs.c` - Filesystem hierarchy

### IPC
- `t_msgget.c`, `t_semget.c`, `t_shmget.c` - IPC primitives
- `t_semop.c`, `t_semflg.c` - Semaphore operations
- `t_shm.c` - Shared memory
- `t_pipe_*.c` - Pipe communication

### Signals
- `t_signal.c`, `t_sigaction.c` - Signal handling
- `t_sigusr.c`, `t_sigfpe.c` - Specific signals
- `t_sigmask.c` - Signal masking

### User/Group Management
- `t_pwd.c`, `t_grp.c` - User/group info
- `t_gid.c`, `t_environ.c` - Environment

### Data Structures
- `t_hashmap.c`, `t_list.c`, `t_ndtree.c` - Built-in data structures

### Filesystem
- `t_ext2_audit_*.c` - EXT2 filesystem auditing

## Building Programs

### Single Program
```bash
make prog_cat        # Build just cat
```

### All Programs
```bash
make programs        # Build all executables
```

### Programs + Tests
```bash
make                 # Build everything
```

## Output Location

Built binaries go to:
```
filesystem/bin/          ← Executables
filesystem/bin/tests/    ← Test binaries
```

These are placed in the filesystem image when you run `make filesystem`.

## Running Programs

### In QEMU
```bash
make qemu      # Run OS in QEMU
login          # At MentOS prompt, login as user
shell> ls      # Run commands
```

### Running Tests
```bash
make qemu-test # Run test suite in QEMU
```

The test runner (`runtests` binary) executes all tests with results.

## Program Structure

Each program is a complete standalone executable:

```c
// userspace/bin/cat.c
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[]) {
    // Read command-line arguments
    // Perform operation
    // Return status
    return 0;
}
```

### Compilation Settings

Each program is compiled with:
- **Text address randomization**: Different programs load at different addresses
- **Static linking**: Linked with libc
- **Custom entry point**: Uses `_start` from libc (`crt0.S`)
- **No standard library**: Uses MentOS libc instead

## Adding New Programs

1. **Create source file**: `userspace/bin/myprog.c`
2. **Update CMakeLists.txt**: Add filename to `PROGRAM_LIST`
3. **Build**: `make`
4. **Binary appears**: `filesystem/bin/myprog`

## Testing Your Program

```bash
# Build
make

# Create filesystem
make filesystem

# Run in QEMU
make qemu

# At shell prompt:
myprog [args...]
```

## Debugging Programs

### Printf Debugging
```c
printf("Debug: value = %d\n", value);
```

### GDB Debugging
```bash
make qemu-gdb          # Start QEMU with GDB wait
```

In another terminal:
```bash
gdb
(gdb) source build/gdb.run
(gdb) break my_function
(gdb) continue
```

## Program Sizes

Text addresses are randomized to avoid symbol collisions:

- **Minimum**: 0x10000000 (256MB)
- **Maximum**: 0xB0000000 (2.75GB)

This allows debugging with GDB while keeping full address space.

## Related

- [ARCHITECTURE.md](../ARCHITECTURE.md) - Project overview
- [lib/README.md](../lib/README.md) - C Library/system calls
- [kernel/README.md](../kernel/README.md) - Kernel
- [shell.c](./bin/shell.c) - Shell implementation (good example)
