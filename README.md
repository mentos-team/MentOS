# MentOS (Mentoring Operating System)

[![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/for-you.svg)](https://forthebadge.com)

[![Ubuntu](https://github.com/mentos-team/MentOS/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/mentos-team/MentOS/actions/workflows/ubuntu.yml)
[![Documentation](https://github.com/mentos-team/MentOS/actions/workflows/documentation.yml/badge.svg)](https://github.com/mentos-team/MentOS/actions/workflows/documentation.yml)

> 📢 **Note:** The default branch is `main`

---

## What is MentOS?

MentOS (Mentoring Operating System) is an **open-source educational operating system** designed for learning OS development. It provides a realistic yet understandable environment that follows Linux design patterns.

### Why MentOS?

- ✅ **Follows Linux guidelines** - Similar data structures and algorithms
- ✅ **Beginner-friendly** - Well-documented, easy to understand
- ✅ **Fully functional** - Real processes, filesystems, drivers, and syscalls
- ✅ **Fast to compile** - Builds in seconds on modern hardware
- ✅ **Hands-on learning** - Modify and extend real OS features

## Quick Start

```bash
# Clone the repository
git clone https://github.com/mentos-team/MentOS.git
cd MentOS

# Build
mkdir build && cd build
cmake ..
make

# Create filesystem
make filesystem

# Run in QEMU
make qemu
```

**Login**: Use `root` or `user` (from `filesystem/etc/passwd`)

## 📚 Documentation

**All documentation is in the [Wiki](https://github.com/mentos-team/MentOS/wiki)**:

### Getting Started

- **[Getting Started](https://github.com/mentos-team/MentOS/wiki/Getting-Started)** - Set up your environment
- **[Building MentOS](https://github.com/mentos-team/MentOS/wiki/Building-MentOS)** - Compile the OS
- **[Running MentOS](https://github.com/mentos-team/MentOS/wiki/Running-MentOS)** - Boot in QEMU or GRUB

### Understanding the Codebase

- **[Architecture](https://github.com/mentos-team/MentOS/wiki/Architecture)** - Project structure
- **[Kernel](https://github.com/mentos-team/MentOS/wiki/Kernel)** - Kernel components and data structures
- **[C Library](https://github.com/mentos-team/MentOS/wiki/C-Library)** - Standard library implementation
- **[System Calls](https://github.com/mentos-team/MentOS/wiki/System-Calls)** - 60+ syscall reference
- **[Filesystem](https://github.com/mentos-team/MentOS/wiki/Filesystem)** - VFS, EXT2, and ProcFS
- **[IPC](https://github.com/mentos-team/MentOS/wiki/IPC)** - Semaphores, message queues, shared memory
- **[Programs](https://github.com/mentos-team/MentOS/wiki/Programs)** - Userspace utilities (40+)

### Development

- **[Development Guide](https://github.com/mentos-team/MentOS/wiki/Development-Guide)** - Add programs and features
- **[Debugging](https://github.com/mentos-team/MentOS/wiki/Debugging)** - GDB and kernel logging
- **[Contributing](https://github.com/mentos-team/MentOS/wiki/Contributing)** - Contribution guidelines
- **[Features](https://github.com/mentos-team/MentOS/wiki/Features)** - Feature roadmap with priorities

## Project Structure

```text
mentos/
├── boot/              ← Bootloader (first code executed)
├── kernel/            ← Core OS (processes, memory, drivers, syscalls)
├── lib/               ← C library + system call wrappers
├── userspace/         ← User programs (40+) and tests (60+)
├── filesystem/        ← Root filesystem content (becomes rootfs.img)
├── iso/               ← GRUB boot configuration
├── doc/               ← Documentation
└── CMakeLists.txt     ← Build configuration
```

## Features

- ✅ Process management (fork, exec, wait, signals)
- ✅ Memory management (paging, buddy system, slab allocator)
- ✅ File systems (VFS, EXT2, procfs)
- ✅ Device drivers (keyboard, ATA, RTC, video)
- ✅ System calls (60+ POSIX-like syscalls)
- ✅ IPC (semaphores, message queues, shared memory)
- ✅ Multiple schedulers (RR, Priority, CFS, EDF, RM, AEDF)
- ✅ User/group management (passwd, shadow, permissions)
- ✅ Shell with pipes and job control
- ✅ 40+ userspace programs (ls, cat, ps, etc.)
- ✅ 60+ test programs

See [Features](https://github.com/mentos-team/MentOS/wiki/Features) for the complete list of implemented features and roadmap.

## Debugging

```bash
# GDB debugging (two terminals needed)
make qemu-gdb          # Terminal 1: Start QEMU
gdb --command=gdb.run  # Terminal 2: Attach GDB
```

See the [Debugging Guide](https://github.com/mentos-team/MentOS/wiki/Debugging) for details.

## Contributing

We welcome contributions! Please see the [Contributing Guide](https://github.com/mentos-team/MentOS/wiki/Contributing).

### Quick contribution steps

1. Fork the repository
2. Create a feature branch: `git checkout -b feature/my-feature`
3. Make your changes following our [coding guidelines](https://github.com/mentos-team/MentOS/wiki/Contributing#code-guidelines)
4. Commit using [Conventional Commits](https://www.conventionalcommits.org/): `git commit -m "feature(scope): description"`
5. Push and open a Pull Request

## Contributors

**Project Manager**: [Enrico Fraccaroli](https://github.com/Galfurian)

**Developers**: [Alessandro Danese](https://github.com/alessandroDanese88), [Luigi Capogrosso](https://github.com/luigicapogrosso), [Mirco De Marchi](https://github.com/mircodemarchi), Andrea Cracco, Linda Sacchetto, Marco Berti, Daniele Nicoletti, Filippo Ziche, and many [valuable contributors](https://github.com/mentos-team/MentOS/graphs/contributors).

## Credits

Parts of MentOS are inherited or inspired by [DreamOS](https://github.com/dreamos82/DreamOs) by Ivan Gualandri.

## License

See [LICENSE.md](LICENSE.md) for details.

---

**Happy hacking! 🎉**
