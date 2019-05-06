MentOS
======

1  What is MentOS
-----------------

An educational 32-bit linux-like Operating System, which integrates the basic
 functionalities of an OS, ideal for an University-level course.
MentOS, which stands for Mentoring OS, is the successor of DreamOS, an
Operating System developed by Ivan Gualandri, Finarfin and many others, which
 can be found at https://github.com/inuyasha82/DreamOs.

2 Developers
----------------
Main Developers:

 * [Enrico Fraccaioli](https://github.com/Galfurian)
 * [Alessandro Danese](https://gitlab.com/ADanese)
 * [Luigi Capogrosso](https://github.com/luigicapogrosso)
 * [Mirco De Marchi](https://github.com/mircodemarchi)

3 Prerequisites
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

4 Compiling MentOS
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

5 See Booting Statistics
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

5 Use Debugger
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
