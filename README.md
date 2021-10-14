# MentOS

[![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/for-you.svg)](https://forthebadge.com)

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

## 2. Prerequisites

MentOS is compatible with the main **unix-based** operating systems. It has been
tested with *Ubuntu*, *WSL1*, *WSL2*, and  *MacOS*.

### 2.1. Generic Prerequisites

#### 3.1.1. Compile

For compiling the system:

 - nasm
 - gcc
 - make
 - cmake
 - git
 - ccmake (suggested)

Under **MacOS**, for compiling, you have additional dependencies:

 - i386-elf-binutils
 - i386-elf-gcc

#### 3.1.2. Execute

To execute the operating system, you need to install:

 - qemu-system-i386 (or qemu-system-x86)

#### 3.1.3. Debug

For debugging we suggest using:

 - cgdb
 - xterm

### 2.2. installation Prerequisites

Under **Ubuntu**, you can type the following commands:

```bash
sudo apt-get update && sudo apt-get upgrade -y
sudo apt-get install -y build-essential git cmake qemu-system-i386 
sudo apt-get install -y cgdb xterm
```

Under **MacOS** you also need to install the i386-elf cross-compiler. The
simplest installation method is through Homebrew package manager.
Install [Homebrew](https://brew.sh/index_it) if you don't already have it, and
then type the following commands:

```bash
brew update && brew upgrade
brew install i386-elf-binutils i386-elf-gcc git cmake qemu nasm
brew install cgdb xterm #<- for debug only
```

## 3. Compiling MentOS

Compile and boot MentOS with qemu:

```bash
cd <clone_directory>
mkdir build
cd build
cmake ..
make
make qemu
```

To login, use one of the usernames listed in `files/etc/passwd`.

## 4. Change the scheduling algorithm

MentOS provides three different scheduling algorithms:

 - Round-Robin
 - Priority
 - Completely Fair Scheduling

If you want to change the scheduling algorithm:

```bash

cd build

# Round Robin scheduling algorithm
cmake -DSCHEDULER_TYPE=SCHEDULER_RR ..
# Priority scheduling algorithm
cmake -DSCHEDULER_TYPE=SCHEDULER_PRIORITY ..
# Completely Fair Scheduling algorithm
cmake -DSCHEDULER_TYPE=SCHEDULER_CFS ..

make
make qemu
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

Select SCHEDULER_TYPE, and type Enter to scroll the three available algorithms
(SCHEDULER_RR, SCHEDULER_PRIORITY, SCHEDULER_CFS). Afterwards,

```bash
<press c>
<press g>
make
make qemu
```

## 5. Use Debugger

If you want to use GDB to debug MentOS:

```bash
cd build
cmake ..
make
make qemu-gdb
```

If you did everything correctly, you should have 3 windows with:

1. Kernel Booting on qemu
2. Shell with video printing of statistics previously discussed
3. Debugger cgdb with code screening

## Developers

Main Developers:

* [Enrico Fraccaroli](https://github.com/Galfurian)
    - Mr. Wolf
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
