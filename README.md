MentOS
======

[![PRs Welcome](https://img.shields.io/appveyor/ci/gruntjs/grunt.svg?style=flat-square)](http://makeapullrequest.com)
[![PRs Welcome](https://img.shields.io/snyk/vulnerabilities/npm/mocha.svg?style=flat-square)](http://makeapullrequest.com)

[![PRs Welcome](https://img.shields.io/bugzilla/996038.svg?style=flat-square)](http://makeapullrequest.com)
[![PRs Welcome](https://img.shields.io/github/issues/detail/state/badges/shields/979.svg?style=flat-square)](http://makeapullrequest.com)
[![PRs Welcome](https://img.shields.io/website/https/shields.io.svg?)](http://makeapullrequest.com)

[![forthebadge](https://forthebadge.com/images/badges/built-with-love.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/made-with-c.svg)](https://forthebadge.com)
[![forthebadge](https://forthebadge.com/images/badges/for-you.svg)](https://forthebadge.com)

What
-----------------

MentOS, which stands for Mentoring OS, is an open source educational 
Operating System.

Goal
-----------------

The goal of MentOS is to provide a project environment that is realisticenough 
to show how a real Operating System work, yet simple enoughthat students can 
understand and modify it in significant ways.

Why
-----------------

There are so many operating systems, why did we write MentOs? 
It is true, there are a lot of education operating system, BUT how many of them 
follow the the guide line defined by Linux? Linux shareswith MentOs the same data 
structures, fields, algorithms, ... it has a sourcecode well documented not requiring 
hours to be compiled on your laptop! 
MentOS team aimed at this, you can download MentOS and you are readyto start in a few minutes, 
search any its data structure with Google, and you will find it in Linux.

Developers
----------------
Main Developers:

 * [Enrico Fraccaroli](https://github.com/Galfurian)
 * [Alessandro Danese](https://gitlab.com/ADanese)
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
make all
make initfs
make qemu
```

If you want to access to the shell, see username and password with:
```
cd <clone_directory>
cat files/passwd
```

See Booting Statistics
-----------------

If you want to see more information about memory allocation, process and file system:
```
cd <clone_directory>
mkdir build
cd build
cmake ..
ccmake ..
```

Now you should see something like this:
```
CMAKE_BUILD_TYPE
CMAKE_INSTALL_PREFIX             /usr/local
ENABLE_QEMU_DEBUG                OFF
```

You have to set ENABLE_QEMU_DEBUG flag on ON:
```
select ENABLE_QEMU_DEBUG entry
click enter
click c
click g
```

There are the instructions of this commands on the bottom of your shell.
It is simple to use. Then, like before:
```
make all
make initfs
make qemu
```

Use Debugger
-----------------
```
cd <clone_directory>
```

Open file .gdb.debug:
```
nano .gdb.debug
```

You should see something like this:
```
symbol-file ./src/mentos/kernel.bin
exec-file   ./src/mentos/kernel.bin
target remote localhost:1234
continue
```

Delete the last line and if you do:
```
cat .gdb.debug
```

You should get:
```
symbol-file ./src/mentos/kernel.bin
exec-file   ./src/mentos/kernel.bin
target remote localhost:1234
```

Now:
```
cd build
cmake ..
ccmake ..
```

Set ON the ENABLE_QEMU_DEBUG: see Section 4. Then:
```
make all
make initfs
make qemu-gdb
```

If you did everything correctly, you should see 3 windows with:
```
1) - Kernel Booting on qemu
2) - Shell with video printing of statistics previously discussed
3) - Debugger cgdb with code screening
```
