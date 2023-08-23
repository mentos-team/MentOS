# MentOS (Mentoring Operating System)

[![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/for-you.svg)](https://forthebadge.com)

[![Ubuntu](https://github.com/mentos-team/MentOS/actions/workflows/ubuntu.yml/badge.svg)](https://github.com/mentos-team/MentOS/actions/workflows/ubuntu.yml)
[![MacOS](https://github.com/mentos-team/MentOS/actions/workflows/macos.yml/badge.svg)](https://github.com/mentos-team/MentOS/actions/workflows/macos.yml)

## Table of Contents

 1. [What is MentOS](#1-what-is-mentos)
 2. [Implemented features](#2-implemented-features)
 3. [Prerequisites](#3-prerequisites)
 4. [Compiling MentOS](#4-compiling-mentos)
 5. [Generating the EXT2 filesystem](#5-generating-the-ext2-filesystem)
 6. [Running MentOS](#6-running-mentos)
 7. [Running MentOS from GRUB](#7-running-mentos-from-grub)
 8. [Running and adding new programs to MentOS](#8-running-and-adding-new-programs-to-mentos)
 9. [Kernel logging](#9-kernel-logging)
 10. [Change the scheduling algorithm](#10-change-the-scheduling-algorithm)
 11. [Debugging the kernel](#11-debugging-the-kernel)
 12. [Contributors](#12-contributors)

## 1. What is MentOS

MentOS (Mentoring Operating System) is an open source educational operating
system. The goal of MentOS is to provide a project environment that is realistic
enough to show how a real Operating System work, yet simple enough that students
can understand and modify it in significant ways.

There are so many operating systems, why did we write MentOS? It is true, there
are a lot of education operating system, BUT how many of them follow the
guideline de fined by Linux?

MentOS aims to have the same Linux's data structures and algorithms. It has a
well-documented source code, and you can compile it on your laptop in a few
seconds!

If you are a beginner in Operating-System developing, perhaps MentOS is the
right operating system to start with.

Parts of MentOS are inherited or inspired by a similar educational operating 
system called [DreamOs](https://github.com/dreamos82/DreamOs) written by Ivan
Gualandri.

*[Back to the Table of Contents](#table-of-contents)*

## 2. Implemented features
Follows the list of implemented features:

**Processes and Events**
 - [x] Memory protection (User vs Kernel);
 - [x] Processes;
 - [x] Scheduler (synchronous and asynchronous);
 - [x] Interrupts and Exceptions;
 - [x] Signals;
 - [x] Timers and RTC;
 - [x] Wait-queues;
 - [x] System Calls;
 - [ ] Multi-core;
 
**Memory**
 - [x] Paging;
 - [x] Buddy System;
 - [x] Slab Allocation;
 - [x] Zone Allocator;
 - [x] Cache Allocator;
 - [x] Heap;
 - [x] Virtual Addressing;

**Filesystem**
 - [x] Virtual Filesystem (VFS);
 - [x] Initramfs;
 - [x] EXT2;
 - [x] Procfs;

**Input/Output**
 - [x] Programmable Interrupt Controller (PIC) drivers;
 - [x] Keyboard drivers (IT/ENG layouts);
 - [x] Video drivers;
 - [ ] VGA drivers;

**Inter-Process Communication (IPC)**
 - [X] Semaphore
 - [ ] Message queue
 - [ ] Shared memory
 - [ ] Named pipe

I will try to keep it updated...

*[Back to the Table of Contents](#table-of-contents)*

## 3. Prerequisites

MentOS is compatible with the main **unix-based** operating systems. It has been
tested with *Ubuntu*, *WSL1*, *WSL2*, and  *MacOS*.

### 3.1. Generic Prerequisites

#### 3.1.1. Compile

For compiling the system:

 - nasm
 - gcc
 - make
 - cmake
 - git
 - ccmake (suggested)
 - e2fsprogs (should be already installed)

Under **MacOS**, for compiling, you have additional dependencies:

 - i386-elf-binutils
 - i386-elf-gcc

#### 3.1.2. Execute

To execute the operating system, you need to install:

 - qemu-system-i386 (or qemu-system-x86)

#### 3.1.3. Debug

For debugging we suggest using:

 - gdb or cgdb

### 3.2. installation Prerequisites

Under **Ubuntu**, you can type the following commands:

```bash
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install -y build-essential git cmake qemu-system-x86 nasm e2fsprogs
sudo apt-get install -y gdb cgdb
```
Note: Older versions might have `qemu-system-i386` instead of `qemu-system-x86`.

Under **MacOS** you also need to install the i386-elf cross-compiler. The
simplest installation method is through Homebrew package manager.
Install [Homebrew](https://brew.sh/index_it) if you don't already have it, and
then type the following commands:

```bash
brew update && brew upgrade
brew install i386-elf-binutils i386-elf-gcc git cmake qemu nasm e2fsprogs
brew install gdb cgdb #<- for debug only
```

*[Back to the Table of Contents](#table-of-contents)*

## 4. Compiling MentOS

Compile MentOS with:

```bash
cd <clone_directory>
mkdir build
cd build
cmake ..
make
```

*[Back to the Table of Contents](#table-of-contents)*

## 5. Generating the EXT2 filesystem

Generate the EXT2 filesystem with:

```bash
make filesystem
```
you just need to generate the filesystem once. If you change a `program` you need to re-generate the entire filesystem with `make filesystem`, but this will override any changes you made to the files inside the `rootfs.img`. In the future I will find a way to update just the `/usr/bin` directory and the programs.

*[Back to the Table of Contents](#table-of-contents)*

## 6. Running MentOS

Boot MentOS with qemu:

```bash
make qemu
```

To login, use one of the usernames listed in `files/etc/passwd`.

*[Back to the Table of Contents](#table-of-contents)*

## 7. Running MentOS from GRUB

For booting MentOS from GRUB in QEMU we need the following tools:

 - xorriso
 - grub-mkrescue (from grub-common)

We also need `grub-pc-bin`, otherwise GRUB won't start in QEMU.

Which can be installed in Ubuntu with the following command:
```bash
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install -y grub-common grub-pc-bin xorriso
```

Boot MentOS with qemu through GRUB by calling:

```bash
make qemu-grub
```

*[Back to the Table of Contents](#table-of-contents)*

## 8. Running and adding new programs to MentOS

This section explains how to add a new program to MentOS, and also how to run programs in mentos. It also explains how to add new tests, which are located in the `programs/tests` folder.

### 8.1. Create a new program

Head to the `programs` (or the `programs/tests`) folder. Create and open a new program, for instance a file called `hello_world.c`, with your preferred editor, and add this content to the file:
```C
#include <stdio.h>

int main(int argc, char *argv[])
{
    printf("Hello, World!\n\n");
    return 0;
}
```

### 8.2. Add the new program to the list of compiled sources
Now we can add the program to the list of files which are compiled and placed inside MentOS filesystem.
The following procedure is the same for both `programs` and `programs/tests`, what changes is which `CMakeLists.txt` file we modify.

You need to modify the `CMakeLists.txt` file, either `programs/CMakeLists.txt` or `programs/tests/CMakeLists.txt`, and add your program to the list of files to be compiled:
```Makefile
# Add the executables (manually).
set(PROGRAMS
    init.c
    ...
    hello_world.c
)
```
or
```Makefile
# Add the executables (manually).
set(TESTS
    t_mem.c
    ...
    hello_world.c
)
```

That's it, the `hello_world.c` file will be compiled and will appear inside the `/bin` or `/bin/tests` folder of MentOS.

### 8.3. Running a program or a test
Once you login into MentOS, if you placed your source code in `programs`, you can execute the program by simply typing:
```bash
hello_world
```
because the file resides in `/bin`, and that folder is listed in the `PATH` environment variable.

Now, if you placed your source code inside the `programs/tests` folder, the executable will end up inside the `/bin/tests` folder.
However, the `/bin/tests` folder is not listed in `PATH`, so, if you want to execute a test from that folder you need to specify the full path:
```bash
/bin/tests/hello_world
```

*[Back to the Table of Contents](#table-of-contents)*

## 9. Kernel logging
The kernel provides ways of printing logging messages *from* inside the kernel code *to* the bash where you executed the `make qemu`.

These *logging* functions are:
```C
#define pr_emerg(...)
#define pr_alert(...)
#define pr_crit(...)
#define pr_err(...)
#define pr_warning(...)
#define pr_notice(...)
#define pr_info(...)
#define pr_debug(...)
#define pr_default(...)
```

You use them like you would use a `printf`:
```C
    if (fd < 0) {
        pr_err("Failed to open file '%s', received file descriptor %d.\n", filename, fd);
        return 1;
    }
```

By default only message that goes from `pr_notice` included down to `pr_emerg` are displayed.

Each logging function (they are actually macros) is a wrapper that automatically sets the desired **log level**. Each log level is identified by a number, and declared as follows:
```C
#define LOGLEVEL_DEFAULT (-1) ///< default-level messages.
#define LOGLEVEL_EMERG   0    ///< system is unusable.
#define LOGLEVEL_ALERT   1    ///< action must be taken immediately.
#define LOGLEVEL_CRIT    2    ///< critical conditions.
#define LOGLEVEL_ERR     3    ///< error conditions.
#define LOGLEVEL_WARNING 4    ///< warning conditions.
#define LOGLEVEL_NOTICE  5    ///< normal but significant condition.
#define LOGLEVEL_INFO    6    ///< informational.
#define LOGLEVEL_DEBUG   7    ///< debug-level messages.
```

You can change the logging level by including the following lines at the beginning of your source code:
```C
// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[ATA   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_INFO
// Include the debuggin header.
#include "io/debug.h"
```
This example sets the `__DEBUG_LEVEL__`, so that all the messages from `INFO` and below are shown. While `__DEBUG_HEADER__` is just a string that is automatically prepended to your message, helping you identifying from which code the message is coming from.

*[Back to the Table of Contents](#table-of-contents)*

## 10. Change the scheduling algorithm

MentOS supports scheduling algorithms for aperiodic:

 - Round-Robin (RR)
 - Highest Priority
 - Completely Fair Scheduling (CFS)
 - Aperiodic Earliest Deadline First (AEDF)

It also supports periodic algorithms:

 - Earliest Deadline First (EDF)
 - Rate Monotonic (RM)

If you want to change the scheduling algorithm, to Round-Robin (RR) for instance:

```bash
cd build
cmake -DSCHEDULER_TYPE=SCHEDULER_RR ..
make
make qemu
```

or you can activate one of the others:

```bash
# Highest Priority
cmake -DSCHEDULER_TYPE=SCHEDULER_PRIORITY ..
# Completely Fair Scheduling (CFS)
cmake -DSCHEDULER_TYPE=SCHEDULER_CFS      ..
# Earliest Deadline First (EDF)
cmake -DSCHEDULER_TYPE=SCHEDULER_EDF      ..
# Rate Monotonic (RM)
cmake -DSCHEDULER_TYPE=SCHEDULER_RM       ..
# Aperiodic Earliest Deadline First (AEDF)
cmake -DSCHEDULER_TYPE=SCHEDULER_AEDF     ..
```

Otherwise you can use `ccmake`:

```bash
cd build
cmake ..
ccmake ..
```

Now you should see something like this:

```
BUILD_DOCUMENTATION              ON
CMAKE_BUILD_TYPE
CMAKE_INSTALL_PREFIX             /usr/local
DEBUGGING_TYPE                   DEBUG_STDIO
ENABLE_BUDDY_SYSTEM              OFF
SCHEDULER_TYPE                   SCHEDULER_RR
```

Select SCHEDULER_TYPE, and type Enter to scroll the three available algorithms (SCHEDULER_RR, SCHEDULER_PRIORITY, SCHEDULER_CFS, SCHEDULER_EDF, SCHEDULER_RM, SCHEDULER_AEDF). Afterwards, you need to

```bash
<press c>
<press g>
make
make qemu
```

*[Back to the Table of Contents](#table-of-contents)*

## 11. Debugging the kernel

If you want to use GDB to debug MentOS, first you need to compile everything:

```bash
cd build
cmake ..
make
```

Then, you need to generate a file called `.gdbinit` placed inside the `build` directory, which will tell **gdb** which *object* file he needs to read in order to allow proper debugging. Basically, it provides for each binary file, the location of their `.text` section. To generate the file, just execute:
```bash
make gdbinit
```

Finally, you run qemu in debugging mode with:
```bash
make qemu-gdb
```

If you did everything correctly, you should see an empty QEMU window. Basically, QEMU is waiting for you to connect *remotely* with gdb. Anyway, running `make qemu-gdb` will make your current shell busy, you cannot call `gdb` in it. You need to open a new shell inside the `build` folder and do a:
```bash
gdb --quiet --command=gdb.run
```
or
```bash
cgdb --quiet --command=gdb.run
```

Now you should have in front of you:
 1. the QEMU window waiting for you;
 2. the **first** shell where you ran `make qemu-gdb`, which is also waiting for you;
 3. the **second** shell where `gdb` is runnign and, you guessed it, is waiting for you.

By default I placed a breakpoint at the begginning of (1) the *bootloader* and (2) the *kernel* itself.

So, when gdb starts you need to first give a continue:
```bash
(gdb) continue
```

This will make the kernel run, and stop at the **first** breakpoint which is inside the *bootloader*:
```bash
Breakpoint 1, boot_main (...) at .../mentos/src/boot.c:220
220     {
```

giving a second `continue` will get you to the start of the operating system:

This will make the kernel run, and stop at the **second** breakpoint which is inside the *kernel*:
```bash
Breakpoint 2, kmain (...) at .../mentos/src/kernel.c:95
95      {
```

There is also a launch configuration for vscode in `.vscode/launch.json`, called `(gdb) Attach`, which should allow you
to connect to the running process.

*[Back to the Table of Contents](#table-of-contents)*

## 12. Contributors

Project Manager:

* [Enrico Fraccaroli](https://github.com/Galfurian)

Developers:
* [Alessandro Danese](https://github.com/alessandroDanese88), [Luigi Capogrosso](https://github.com/luigicapogrosso), [Mirco De Marchi](https://github.com/mircodemarchi)
    - Protection ring
    - libc
* Andrea Cracco
    - Buddy System, Heap, Paging, Slab, Caching, Zone
    - Process Image, ELF
    - VFS: procfs
    - Bootloader
* Linda Sacchetto, Marco Berti
    - Real time scheduler
* Daniele Nicoletti, Filippo Ziche
    - Real time scheduler (Asynchronous EDF)
    - Soft IRQs
    - Timer
    - Signals

*[Back to the Table of Contents](#table-of-contents)*
