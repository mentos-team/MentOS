MentOS
======

[![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/for-you.svg)](https://forthebadge.com)

What is MentOS
-----------------

MentOS (Mentoring Operating system) is an open source educational operating
system.
The goal of MentOS is to provide a project environment that is realistic
enough to show how a real Operating System work, yet simple enough that
students can understand and modify it in significant ways.

There are so many operating systems, why did we write MentOS?
It is true, there are a lot of education operating system, BUT
how many of them follow the guideline defined by Linux?

MentOS aims to have the same Linux's data structures and algorithms. It
has a well-documented source code, and you can compile it on your laptop
in a few seconds!
If you are a beginner in Operating-System developing, perhaps MentOS is the
right operating system to start with.


Developers
----------------
Main Developers:

 * [Enrico Fraccaroli](https://github.com/Galfurian)
 * [Alessandro Danese](https://github.com/alessandroDanese88)
 * [Luigi Capogrosso](https://github.com/luigicapogrosso)
 * [Mirco De Marchi](https://github.com/mircodemarchi)

Prerequisites
-----------------
For compiling the main system:

 * nasm
 * gcc
 * g++
 * make
 * cmake
 * git

To run and try:

 * qemu-system-x86

For debugging:

 * ccmake
 * gdb or cgdb
 * xterm

Compiling MentOS
-----------------
Compile and boot MentOS with qemu:

```
cd <clone_directory>
mkdir build
cd build
cmake ..
make
make qemu
```

If you want to access to the shell, use one of the usernames listed in files/passwd.

Change the scheduling algorithm
-----------------

MentOS provides three different scheduling algorithms:

* Round-Robin
* Priority
* Completely Fair Scheduling

If you want to change the scheduling algorithm:
```
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
(SCHEDULER_RR, SCHEDULER_PRIORITY, SCHEDULER_CFS).
Afterwards,
```
type c
type g
make
make qemu
```

Enable to Buddy System
-----------------

MentOS provides a Buddy System to manage the allocation and deallocation of
page frames in the physical memory.

If you want to enable the MentOS's Buddy System:
```
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

Select ENABLE_BUDDY_SYSTEM, and type Enter.
You should see something like this:
```
BUILD_DOCUMENTATION              ON
CMAKE_BUILD_TYPE
CMAKE_INSTALL_PREFIX             /usr/local
DEBUGGING_TYPE                   DEBUG_STDIO
ENABLE_BUDDY_SYSTEM              ON
SCHEDULER_TYPE                   SCHEDULER_RR
```

Afterwards,
```
type c
type g
make
make qemu
```

Use Debugger
-----------------
If you want to use GDB to debug MentOS:
```
cd build
cmake ..
make
make qemu-gdb
```

If you did everything correctly, you should have 3 windows with:
```
1) - Kernel Booting on qemu
2) - Shell with video printing of statistics previously discussed
3) - Debugger cgdb with code screening
```
