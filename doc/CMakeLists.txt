# Find Doxygen
find_package(Doxygen)

if (DOXYGEN_FOUND)
    # Read the file with the version.
    file(READ ${CMAKE_SOURCE_DIR}/mentos/inc/version.h version_file)
    # Extract the OS version.
    string(REGEX MATCH "OS_MAJOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MAJOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MINOR_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MINOR_VERSION ${CMAKE_MATCH_1})
    string(REGEX MATCH "OS_MICRO_VERSION ([0-9]*)" _ ${version_file})
    set(OS_MICRO_VERSION ${CMAKE_MATCH_1})
    # Setup the Doxygen documentation.
    set(DOXYGEN_PROJECT_NAME "MentOS")
    set(DOXYGEN_PROJECT_NUMBER "${OS_MAJOR_VERSION}.${OS_MINOR_VERSION}.${OS_MICRO_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF "The Mentoring Operating System")
    set(DOXYGEN_USE_MDFILE_AS_MAINPAGE ${CMAKE_SOURCE_DIR}/README.md)
    set(DOXYGEN_IMAGE_PATH ${CMAKE_SOURCE_DIR}/doc/resources)
    set(DOXYGEN_SHOW_INCLUDE_FILES NO)
    set(DOXYGEN_GENERATE_TREEVIEW NO)
    set(DOXYGEN_WARN_NO_PARAMDOC YES)

    set(DOXYGEN_ENABLE_PREPROCESSING YES)
    set(DOXYGEN_EXTRACT_STATIC YES)
    set(DOXYGEN_MACRO_EXPANSION YES)
    set(DOXYGEN_EXPAND_ONLY_PREDEF YES)
    set(DOXYGEN_PREDEFINED "__attribute__((x))= _syscall0= _syscall0(x)= _syscall1(x)= _syscall2(x)= _syscall3(x)=")

    set(DOXYGEN_WARN_FORMAT "$file($line): $text")
    set(DOXYGEN_HTML_STYLESHEET ${CMAKE_SOURCE_DIR}/doc/doxygen.css)
    doxygen_add_docs(
        mentos_documentation
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE.md
        ${CMAKE_SOURCE_DIR}/doc/signal.md
        ${CMAKE_SOURCE_DIR}/doc/syscall.md

        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/rtc.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/ps2.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/fdc.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/mouse.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/mem.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/ata/ata_types.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/ata/ata.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/keyboard/keymap.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/drivers/keyboard/keyboard.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/elf/elf.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/multiboot.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/process/scheduler_feedback.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/process/prio.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/process/process.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/process/wait.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/process/scheduler.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/version.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/proc_access.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/gfp.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/zone_allocator.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/paging.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/slab.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/kheap.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/vmem_map.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/mem/buddysystem.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/hardware/timer.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/hardware/cpuid.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/hardware/pic8259.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/kernel.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/link_access.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/descriptor_tables/gdt.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/descriptor_tables/tss.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/descriptor_tables/isr.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/descriptor_tables/idt.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/ipc/ipc.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/irqflags.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/spinlock.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/stdatomic.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/stack_helper.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/mutex.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/compiler.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/klib/rbtree.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/boot.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/sys/module.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/vga/vga_font.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/vga/vga.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/vga/vga_palette.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/vga/vga_mode.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/proc_modules.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/video.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/io/debug.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/devices/pci.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/devices/fpu.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/system/printk.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/system/panic.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/system/syscall.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/system/signal.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/vfs.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/ext2.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/pipe.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/namei.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/attr.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/procfs.h
        ${CMAKE_SOURCE_DIR}/mentos/inc/fs/vfs_types.h
        ${CMAKE_SOURCE_DIR}/mentos/kernel.lds
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/mouse.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/rtc.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/mem.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/fdc.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/keyboard/keymap.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/keyboard/keyboard.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/ps2.c
        ${CMAKE_SOURCE_DIR}/mentos/src/drivers/ata.c
        ${CMAKE_SOURCE_DIR}/mentos/src/elf/elf.c
        ${CMAKE_SOURCE_DIR}/mentos/src/process/scheduler_feedback.c
        ${CMAKE_SOURCE_DIR}/mentos/src/process/process.c
        ${CMAKE_SOURCE_DIR}/mentos/src/process/user.S
        ${CMAKE_SOURCE_DIR}/mentos/src/process/scheduler_algorithm.c
        ${CMAKE_SOURCE_DIR}/mentos/src/process/scheduler.c
        ${CMAKE_SOURCE_DIR}/mentos/src/process/wait.c
        ${CMAKE_SOURCE_DIR}/mentos/src/crypt/sha256.c
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/zone_allocator.c
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/paging.c
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/buddysystem.c
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/libbuddysystem.a
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/kheap.c
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/slab.c
        ${CMAKE_SOURCE_DIR}/mentos/src/mem/vmem_map.c
        ${CMAKE_SOURCE_DIR}/mentos/src/hardware/timer.c
        ${CMAKE_SOURCE_DIR}/mentos/src/hardware/pic8259.c
        ${CMAKE_SOURCE_DIR}/mentos/src/hardware/cpuid.c
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/exception.S
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/idt.S
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/interrupt.S
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/gdt.c
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/interrupt.c
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/exception.c
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/gdt.S
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/tss.c
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/idt.c
        ${CMAKE_SOURCE_DIR}/mentos/src/descriptor_tables/tss.S
        ${CMAKE_SOURCE_DIR}/mentos/src/ipc/sem.c
        ${CMAKE_SOURCE_DIR}/mentos/src/ipc/msg.c
        ${CMAKE_SOURCE_DIR}/mentos/src/ipc/ipc.c
        ${CMAKE_SOURCE_DIR}/mentos/src/ipc/shm.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/stdlib.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/rbtree.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/vsprintf.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/fcvt.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/vscanf.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/time.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/libgen.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/string.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/mutex.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/hashmap.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/ndtree.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/assert.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/list.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/strerror.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/ctype.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/spinlock.c
        ${CMAKE_SOURCE_DIR}/mentos/src/klib/math.c
        ${CMAKE_SOURCE_DIR}/mentos/src/boot.S
        ${CMAKE_SOURCE_DIR}/mentos/src/sys/utsname.c
        ${CMAKE_SOURCE_DIR}/mentos/src/sys/module.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/debug.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/mm_io.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/vga/vga.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/stdio.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/video.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_video.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_running.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_feedback.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_ipc.c
        ${CMAKE_SOURCE_DIR}/mentos/src/io/proc_system.c
        ${CMAKE_SOURCE_DIR}/mentos/src/kernel.c
        ${CMAKE_SOURCE_DIR}/mentos/src/devices/pci.c
        ${CMAKE_SOURCE_DIR}/mentos/src/devices/fpu.c
        ${CMAKE_SOURCE_DIR}/mentos/src/boot.c
        ${CMAKE_SOURCE_DIR}/mentos/src/system/signal.c
        ${CMAKE_SOURCE_DIR}/mentos/src/system/errno.c
        ${CMAKE_SOURCE_DIR}/mentos/src/system/panic.c
        ${CMAKE_SOURCE_DIR}/mentos/src/system/syscall.c
        ${CMAKE_SOURCE_DIR}/mentos/src/system/printk.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/open.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/readdir.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/ext2.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/read_write.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/fcntl.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/stat.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/ioctl.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/vfs.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/attr.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/pipe.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/procfs.c
        ${CMAKE_SOURCE_DIR}/mentos/src/fs/namei.c
        ${CMAKE_SOURCE_DIR}/mentos/src/multiboot.c
        ${CMAKE_SOURCE_DIR}/mentos/src/kernel/sys.c

        ${CMAKE_SOURCE_DIR}/libc/inc/errno.h
        ${CMAKE_SOURCE_DIR}/libc/inc/limits.h
        ${CMAKE_SOURCE_DIR}/libc/inc/stdbool.h
        ${CMAKE_SOURCE_DIR}/libc/inc/stdint.h
        ${CMAKE_SOURCE_DIR}/libc/inc/time.h
        ${CMAKE_SOURCE_DIR}/libc/inc/bits/termios-struct.h
        ${CMAKE_SOURCE_DIR}/libc/inc/bits/ioctls.h
        ${CMAKE_SOURCE_DIR}/libc/inc/bits/stat.h
        ${CMAKE_SOURCE_DIR}/libc/inc/shadow.h
        ${CMAKE_SOURCE_DIR}/libc/inc/fcvt.h
        ${CMAKE_SOURCE_DIR}/libc/inc/string.h
        ${CMAKE_SOURCE_DIR}/libc/inc/pwd.h
        ${CMAKE_SOURCE_DIR}/libc/inc/readline.h
        ${CMAKE_SOURCE_DIR}/libc/inc/crypt/sha256.h
        ${CMAKE_SOURCE_DIR}/libc/inc/termios.h
        ${CMAKE_SOURCE_DIR}/libc/inc/syslog.h
        ${CMAKE_SOURCE_DIR}/libc/inc/stdlib.h
        ${CMAKE_SOURCE_DIR}/libc/inc/fcntl.h
        ${CMAKE_SOURCE_DIR}/libc/inc/stdio.h
        ${CMAKE_SOURCE_DIR}/libc/inc/ring_buffer.h
        ${CMAKE_SOURCE_DIR}/libc/inc/err.h
        ${CMAKE_SOURCE_DIR}/libc/inc/stdarg.h
        ${CMAKE_SOURCE_DIR}/libc/inc/libgen.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sched.h
        ${CMAKE_SOURCE_DIR}/libc/inc/assert.h
        ${CMAKE_SOURCE_DIR}/libc/inc/stddef.h
        ${CMAKE_SOURCE_DIR}/libc/inc/signal.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/shm.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/bitops.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/types.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/sem.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/reboot.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/msg.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/ipc.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/wait.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/kernel_levels.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/ioctl.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/mman.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/utsname.h
        ${CMAKE_SOURCE_DIR}/libc/inc/sys/stat.h
        ${CMAKE_SOURCE_DIR}/libc/inc/list_head.h
        ${CMAKE_SOURCE_DIR}/libc/inc/grp.h
        ${CMAKE_SOURCE_DIR}/libc/inc/ndtree.h
        ${CMAKE_SOURCE_DIR}/libc/inc/io/ansi_colors.h
        ${CMAKE_SOURCE_DIR}/libc/inc/io/mm_io.h
        ${CMAKE_SOURCE_DIR}/libc/inc/io/port_io.h
        ${CMAKE_SOURCE_DIR}/libc/inc/ctype.h
        ${CMAKE_SOURCE_DIR}/libc/inc/dirent.h
        ${CMAKE_SOURCE_DIR}/libc/inc/hashmap.h
        ${CMAKE_SOURCE_DIR}/libc/inc/system/syscall_types.h
        ${CMAKE_SOURCE_DIR}/libc/inc/list.h
        ${CMAKE_SOURCE_DIR}/libc/inc/array.h
        ${CMAKE_SOURCE_DIR}/libc/inc/strerror.h
        ${CMAKE_SOURCE_DIR}/libc/inc/math.h
        ${CMAKE_SOURCE_DIR}/libc/inc/unistd.h
        ${CMAKE_SOURCE_DIR}/libc/inc/list_head_algorithm.h
        ${CMAKE_SOURCE_DIR}/libc/src/stdlib.c
        ${CMAKE_SOURCE_DIR}/libc/src/libc_start.c
        ${CMAKE_SOURCE_DIR}/libc/src/setenv.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/signal.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/reboot.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/exit.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/fork.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/setuid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/open.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/kill.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getppid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/interval.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/symlink.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getsid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/read.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/fcntl.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/readlink.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/chmod.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/mkdir.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getdents.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/rmdir.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/setsid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/stat.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getgid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/dup.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getpid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getuid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/exec.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getcwd.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/setpgid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/close.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/waitpid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/chown.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/pipe.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/creat.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/nice.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/lseek.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/setgid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/getpgid.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/chdir.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/unlink.c
        ${CMAKE_SOURCE_DIR}/libc/src/unistd/write.c
        ${CMAKE_SOURCE_DIR}/libc/src/vsprintf.c
        ${CMAKE_SOURCE_DIR}/libc/src/fcvt.c
        ${CMAKE_SOURCE_DIR}/libc/src/abort.c
        ${CMAKE_SOURCE_DIR}/libc/src/stdio.c
        ${CMAKE_SOURCE_DIR}/libc/src/vscanf.c
        ${CMAKE_SOURCE_DIR}/libc/src/time.c
        ${CMAKE_SOURCE_DIR}/libc/src/err.c
        ${CMAKE_SOURCE_DIR}/libc/src/crypt/sha256.c
        ${CMAKE_SOURCE_DIR}/libc/src/pwd.c
        ${CMAKE_SOURCE_DIR}/libc/src/libgen.c
        ${CMAKE_SOURCE_DIR}/libc/src/string.c
        ${CMAKE_SOURCE_DIR}/libc/src/crt0.S
        ${CMAKE_SOURCE_DIR}/libc/src/hashmap.c
        ${CMAKE_SOURCE_DIR}/libc/src/ndtree.c
        ${CMAKE_SOURCE_DIR}/libc/src/assert.c
        ${CMAKE_SOURCE_DIR}/libc/src/grp.c
        ${CMAKE_SOURCE_DIR}/libc/src/shadow.c
        ${CMAKE_SOURCE_DIR}/libc/src/sched.c
        ${CMAKE_SOURCE_DIR}/libc/src/list.c
        ${CMAKE_SOURCE_DIR}/libc/src/syslog.c
        ${CMAKE_SOURCE_DIR}/libc/src/strerror.c
        ${CMAKE_SOURCE_DIR}/libc/src/sys/mman.c
        ${CMAKE_SOURCE_DIR}/libc/src/sys/errno.c
        ${CMAKE_SOURCE_DIR}/libc/src/sys/ipc.c
        ${CMAKE_SOURCE_DIR}/libc/src/sys/unistd.c
        ${CMAKE_SOURCE_DIR}/libc/src/sys/ioctl.c
        ${CMAKE_SOURCE_DIR}/libc/src/sys/utsname.c
        ${CMAKE_SOURCE_DIR}/libc/src/ctype.c
        ${CMAKE_SOURCE_DIR}/libc/src/readline.c
        ${CMAKE_SOURCE_DIR}/libc/src/termios.c
        ${CMAKE_SOURCE_DIR}/libc/src/io/mm_io.c
        ${CMAKE_SOURCE_DIR}/libc/src/math.c
    )
endif (DOXYGEN_FOUND)