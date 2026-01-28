# TODO - MentOS Feature Roadmap

This file tracks features organized by priority. See [Features Wiki](https://github.com/mentos-team/MentOS/wiki/Features) for detailed descriptions.

## High Priority

### Processes and Events
- [x] Memory protection (User vs Kernel mode)
- [x] Process management (fork, exec, exit, wait)
- [x] Scheduler (multiple algorithms: RR, Priority, CFS, EDF, RM, AEDF)
- [x] Interrupts and exceptions
- [x] Signals (POSIX-like)
- [x] Timers and RTC
- [x] Wait queues
- [x] System calls (60+ implemented)
- [ ] Multi-core support (SMP)
- [ ] Thread support (POSIX threads)

### Memory Management
- [x] Paging (virtual memory)
- [x] Buddy system (physical allocator)
- [x] Slab allocation
- [x] Zone allocator
- [x] Cache allocator
- [x] Heap management
- [x] Virtual addressing
- [x] Memory mapping (mmap/munmap)
- [ ] Swapping (disk-based virtual memory)
- [ ] Huge pages (2MB/4MB pages)
- [ ] Memory cgroups

### File Systems
- [x] Virtual Filesystem (VFS)
- [x] Initramfs
- [x] EXT2 filesystem
- [x] Procfs
- [ ] Pipes (anonymous pipes for IPC)
- [ ] Named pipes (FIFOs)
- [ ] Tmpfs (RAM-based filesystem)
- [ ] FAT32 support
- [ ] EXT3/EXT4 (journaling)
- [ ] Symlinks

### Input/Output
- [x] PIC drivers
- [x] PS/2 drivers
- [x] ATA drivers (IDE)
- [x] RTC drivers
- [x] Keyboard drivers (IT/US layouts)
- [x] Video drivers (text mode)
- [ ] VGA drivers (graphics mode)
- [ ] AHCI drivers (SATA)
- [ ] USB drivers
- [ ] Network drivers (Ethernet)
- [ ] Sound drivers

## Medium Priority

### Inter-Process Communication
- [x] Semaphores (System V)
- [x] Message queues (System V)
- [x] Shared memory (System V)
- [ ] POSIX semaphores
- [ ] POSIX message queues
- [ ] Pipes (anonymous)
- [ ] Named pipes (FIFOs)
- [ ] Unix domain sockets
- [ ] Enhanced signal handling

### Security
- [x] User/group management
- [x] File permissions (Unix-style)
- [x] Process privileges (UID/GID)
- [ ] Capabilities (fine-grained privileges)
- [ ] SELinux
- [ ] Seccomp (syscall filtering)
- [ ] ASLR (Address Space Layout Randomization)

### Networking
- [ ] Network stack (TCP/IP)
- [ ] Socket API (Berkeley sockets)
- [ ] Ethernet driver
- [ ] IP protocol
- [ ] TCP protocol
- [ ] UDP protocol
- [ ] ICMP (ping)
- [ ] DNS resolution

### Development Tools
- [x] GDB support
- [x] Serial console output
- [x] Kernel logging (pr_debug, pr_info, etc.)
- [x] Build system (CMake)
- [ ] Kernel profiler
- [ ] Memory leak detector
- [ ] Code coverage tools

## Low Priority

### Advanced Scheduling
- [x] Round-Robin (RR)
- [x] Priority-based
- [x] Completely Fair Scheduler (CFS)
- [x] Earliest Deadline First (EDF)
- [x] Rate Monotonic (RM)
- [x] Adaptive EDF (AEDF)
- [ ] O(1) scheduler
- [ ] CPU affinity
- [ ] cgroups (control groups)
- [ ] Nice values (runtime adjustment)

### Power Management
- [ ] ACPI support
- [ ] CPU frequency scaling
- [ ] Suspend/resume
- [ ] Hibernation
- [ ] Power profiles

### Graphics and UI
- [ ] Framebuffer support
- [ ] VGA graphics mode
- [ ] Windowing system
- [ ] GUI framework
- [ ] Mouse cursor
- [ ] Graphical terminal emulator

### System Services
- [x] init system (PID 1)
- [x] Login/authentication
- [x] Shell (command-line interface)
- [ ] Daemon management (systemd-like)
- [ ] Cron (scheduled tasks)
- [ ] Syslog (system logging daemon)

### Userspace Programs
- [x] Core utilities (40+ programs: cat, ls, mkdir, rm, cp, etc.)
- [x] System tools (ps, kill, uptime, uname, etc.)
- [x] Text editor (edit)
- [x] Manual pages (man)
- [ ] Package manager
- [ ] Compiler toolchain (gcc, ld)
- [ ] Text processing (grep, sed, awk)
- [ ] Scripting language (sh, python)

### Testing and Quality
- [x] Test suite (60+ tests)
- [x] CI/CD pipeline (GitHub Actions)
- [x] Documentation (Doxygen)
- [ ] Unit tests (kernel unit tests)
- [ ] Integration tests (automated)
- [ ] Stress tests (memory, CPU, I/O)
- [ ] Fuzzing (security testing)

### Miscellaneous
- [ ] Loadable kernel modules (LKM)
- [ ] Dynamic linking (shared libraries)
- [ ] Kernel configuration (Kconfig-like)
- [ ] Hot-plugging (USB, PCI)
- [ ] Virtualization support (KVM-like)
- [ ] Container support (namespaces)

---

## Near-term Roadmap (Next 6 Months)

Priority tasks for upcoming development:

1. [ ] Complete pipe implementation
2. [ ] Add named pipes (FIFOs)
3. [ ] Improve VGA driver
4. [ ] Basic network stack (IP/TCP)
5. [ ] Thread support

## Mid-term Roadmap (6-12 Months)

1. [ ] Multi-core support (SMP)
2. [ ] Advanced scheduling (CPU affinity)
3. [ ] More filesystems (FAT32, tmpfs)
4. [ ] USB support
5. [ ] Sound drivers

## Long-term Roadmap (12+ Months)

1. [ ] Graphics subsystem
2. [ ] Windowing system
3. [ ] Package manager
4. [ ] Compiler toolchain
5. [ ] Container support

---

## Contributing

Want to help implement a feature? See [Contributing Guide](https://github.com/mentos-team/MentOS/wiki/Contributing).

### Suggested Starter Features

- [ ] Named pipes (medium difficulty)
- [ ] Tmpfs (medium difficulty)
- [ ] Additional keyboard layouts (easy)
- [ ] More test programs (easy)
- [ ] Documentation improvements (easy)

### Feature Requests

Have an idea? [Open an issue](https://github.com/mentos-team/MentOS/issues) with:
- Feature description
- Use case
- Priority justification

---

**Status**: ~50 features implemented, ~80 features planned (~38% complete)

Last updated: January 2026
