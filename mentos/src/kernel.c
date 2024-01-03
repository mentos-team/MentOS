/// @file   kernel.c
/// @brief  Kernel main function.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[KERNEL]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "kernel.h"

#include "descriptor_tables/gdt.h"
#include "descriptor_tables/idt.h"
#include "drivers/ata/ata.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keymap.h"
#include "drivers/ps2.h"
#include "drivers/rtc.h"
#include "drivers/mem.h"
#include "fs/ext2.h"
#include "fs/procfs.h"
#include "fs/vfs.h"
#include "hardware/pic8259.h"
#include "hardware/timer.h"
#include "io/proc_modules.h"
#include "io/vga/vga.h"
#include "io/video.h"
#include "mem/vmem_map.h"
#include "mem/zone_allocator.h"
#include "process/scheduler.h"
#include "process/scheduler_feedback.h"
#include "stdio.h"
#include "sys/module.h"
#include "sys/msg.h"
#include "sys/sem.h"
#include "sys/shm.h"
#include "system/syscall.h"
#include "version.h"

/// Describe start address of grub multiboot modules.
char *module_start[MAX_MODULES];
/// Describe end address of grub multiboot modules.
char *module_end[MAX_MODULES];

// Everything is defined in kernel.ld.

/// Points at the multiheader grub info, starting address.
extern uint32_t _multiboot_header_start;
/// Points at the multiheader grub info, ending address.
extern uint32_t _multiboot_header_end;
/// Points at the kernel code, starting address.
extern uint32_t _text_start;
/// Points at the kernel code, ending address.
extern uint32_t _text_end;
/// Points at the read-only kernel data, starting address.
extern uint32_t _rodata_start;
/// Points at the read-only kernel data, ending address.
extern uint32_t _rodata_end;
/// Points at the read-write kernel data initialized, starting address.
extern uint32_t _data_start;
/// Points at the read-write kernel data initialized, ending address.
extern uint32_t _data_end;
/// Points at the read-write kernel data uninitialized an kernel stack, starting address.
extern uint32_t _bss_start;
/// Points at the read-write kernel data uninitialized an kernel stack, ending address.
extern uint32_t _bss_end;
/// Points at the top of the kernel stack.
extern uint32_t stack_top;
/// Points at the bottom of the kernel stack.
extern uint32_t stack_bottom;
/// Points at the end of kernel code/data.
extern uint32_t end;

/// Initial ESP.
uintptr_t initial_esp = 0;
/// The boot info.
boot_info_t boot_info;

/// @brief Prints [OK] at the current row and column 60.
static inline void print_ok(void)
{
    unsigned y, width;
    video_get_cursor_position(NULL, &y);
    video_get_screen_size(&width, NULL);
    video_move_cursor(width - 5, y);
    video_puts("[OK]\n");
}

/// @brief Prints [FAIL] at the current row and column 60.
static inline void print_fail(void)
{
    unsigned y, width;
    video_get_cursor_position(NULL, &y);
    video_get_screen_size(&width, NULL);
    video_move_cursor(width - 7, y);
    video_puts("[FAIL]\n");
}

/// @brief Entry point of the kernel.
/// @param boot_informations Information concerning the boot.
/// @return The exit status of the kernel.
int kmain(boot_info_t *boot_informations)
{
    pr_notice("Booting...\n");
    // Make a copy for when paging is enabled
    boot_info = *boot_informations;
    // Am I booted by a Multiboot-compliant boot loader?
    if (boot_info.magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        printf("Invalid magic number: 0x%x\n", boot_info.magic);
        return 1;
    }
    // Set the initial esp.
    initial_esp = boot_info.stack_base;
    // Dump the multiboot structure.
    dump_multiboot(boot_info.multiboot_header);

    //==========================================================================
    pr_notice("Initialize the video...\n");
    vga_initialize();
    video_init();

    //==========================================================================
    printf(OS_NAME " " OS_VERSION);
    printf("\nSite:");
    printf(OS_SITEURL);
    printf("\n\n");

    //==========================================================================
    pr_notice("Initialize modules...\n");
    printf("Initialize modules...");
    if (!init_modules(boot_info.multiboot_header)) {
        print_fail();
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize physical memory manager...\n");
    printf("Initialize physical memory manager...");
    if (!pmmngr_init(&boot_info)) {
        print_fail();
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize slab allocator.\n");
    printf("Initialize slab...");
    kmem_cache_init();
    print_ok();

    //==========================================================================
    pr_notice("Initialize Global Descriptor Table (GDT)...\n");
    printf("Initialize GDT...");
    init_gdt();
    print_ok();

    //==========================================================================
    pr_notice("Initialize Interrupt Service Routine(ISR)...\n");
    printf("Initialize IDT...");
    init_idt();
    print_ok();

    //==========================================================================
    pr_notice("Initialize system calls...\n");
    printf("Initialize system calls...");
    syscall_init();
    print_ok();

    //==========================================================================
    pr_notice("Initialize IRQ...\n");
    printf("Initialize IRQ...");
    pic8259_init_irq();
    print_ok();

    //==========================================================================
    pr_notice("Relocate modules.\n");
    printf("Relocate modules...");
    relocate_modules();
    print_ok();

    //==========================================================================
    pr_notice("Initialize paging.\n");
    printf("Initialize paging...");
    paging_init(&boot_info);
    print_ok();

    //==========================================================================
    pr_notice("Initialize virtual memory mapping.\n");
    printf("Initialize virtual memory mapping...");
    virt_init();
    print_ok();

    //==========================================================================
    pr_notice("Install the timer.\n");
    printf("Setting up timer...");
    timer_install();
    print_ok();

    //==========================================================================
    pr_notice("Install RTC.\n");
    printf("Setting up RTC...");
    rtc_initialize();
    print_ok();

    //==========================================================================
    pr_notice("Initialize the filesystem.\n");
    printf("Initialize the filesystem...");
    vfs_init();
    print_ok();

    //==========================================================================
    // Scan for ata devices.
    pr_notice("Initialize ATA devices...\n");
    printf("Initialize ATA devices...");
    if (ata_initialize()) {
        pr_emerg("Failed to initialize ATA devices!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize EXT2 filesystem...\n");
    printf("Initialize EXT2 filesystem...");
    if (ext2_initialize()) {
        pr_emerg("Failed to initialize EXT2 filesystem!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Mount EXT2 filesystem...\n");
    printf("Mount EXT2 filesystem...");
    if (do_mount("ext2", "/", "/dev/hda")) {
        pr_emerg("Failed to mount EXT2 filesystem...\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("    Initialize memory devices...\n");
    printf("    Initialize memory devices...");
    if (mem_devs_initialize()) {
        print_fail();
        pr_emerg("Failed to initialize memory devices!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("    Initialize 'procfs'...\n");
    printf("    Initialize 'procfs'...");
    if (procfs_module_init()) {
        print_fail();
        pr_emerg("Failed to register `procfs`!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("    Mounting 'procfs'...\n");
    printf("    Mounting 'procfs'...");
    if (do_mount("procfs", "/proc", NULL)) {
        pr_emerg("Failed to mount procfs at `/proc`!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize video procfs file...\n");
    printf("Initialize video procfs file...");
    if (procv_module_init()) {
        print_fail();
        pr_emerg("Failed to initialize `/proc/video`!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize system procfs file...\n");
    printf("Initialize system procfs file...");
    if (procs_module_init()) {
        print_fail();
        pr_emerg("Failed to initialize proc system entries!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize IPC information system...\n");
    printf("Initialize IPC information system...");
    if (procipc_module_init()) {
        print_fail();
        pr_emerg("Failed to initialize the IPC information system!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize IPC/SEM system...\n");
    printf("Initialize IPC/SEM system...");
    if (sem_init()) {
        print_fail();
        pr_emerg("Failed to initialize the IPC/SEM system!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize IPC/MSQ system...\n");
    printf("Initialize IPC/MSQ system...");
    if (msq_init()) {
        print_fail();
        pr_emerg("Failed to initialize the IPC/MSQ system!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize IPC/SHM system...\n");
    printf("Initialize IPC/SHM system...");
    if (shm_init()) {
        print_fail();
        pr_emerg("Failed to initialize the IPC/SHM system!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    // First, disable the keyboard, otherwise the PS/2 initialization does not
    // work properly.
    keyboard_disable();

    //==========================================================================
    pr_notice("Setting up PS/2 driver...\n");
    printf("Setting up PS/2 driver...");
    if (ps2_initialize()) {
        print_fail();
        pr_emerg("Failed to initialize proc system entries!\n");
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Setting up keyboard driver...\n");
    printf("Setting up keyboard driver...");
    keyboard_initialize();
    print_ok();
    // Set the keymap type.
#ifdef USE_KEYMAP_US
    set_keymap_type(KEYMAP_US);
#elif USE_KEYMAP_DE
    set_keymap_type(KEYMAP_DE);
#else
    set_keymap_type(KEYMAP_IT);
#endif

    keyboard_enable();

    //==========================================================================
#if 0
     pr_notice("Install the mouse.\n");
     printf(" * Setting up mouse driver...");
     mouse_install();     // Install the mouse.
     print_ok();
#endif

    //==========================================================================
    pr_notice("Initialize the scheduler.\n");
    printf("Initialize the scheduler...");
    scheduler_initialize();
    print_ok();

    //==========================================================================
    pr_notice("Init process management...\n");
    printf("Init process management...");
    if (!init_tasking()) {
        print_fail();
        return 1;
    }
    print_ok();

    //==========================================================================
#ifdef ENABLE_SCHEDULER_FEEDBACK
    pr_notice("Initialize scheduler feedback system...\n");
    printf("Initialize scheduler feedback system...");
    if (!scheduler_feedback_init()) {
        print_fail();
        return 1;
    }
    print_ok();
#endif

    //==========================================================================
    pr_notice("Initialize scheduler feedback system (2)...\n");
    printf("Initialize scheduler feedback system (2)...");
    if (procfb_module_init()) {
        print_fail();
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Creating init process...\n");
    printf("Creating init process...");
    task_struct *init_p = process_create_init("/bin/init");
    if (!init_p) {
        print_fail();
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize floating point unit...\n");
    printf("Initialize floating point unit...");
    if (!fpu_install()) {
        print_fail();
        return 1;
    }
    print_ok();

    //==========================================================================
    pr_notice("Initialize signals...\n");
    printf("Initialize signals...");
    if (!signals_init()) {
        print_fail();
        return 1;
    }
    print_ok();

    // We have completed the booting procedure.
    pr_notice("Booting done, jumping into init process.\n");
    // Switch to the page directory of init.
    paging_switch_directory_va(init_p->mm->pgd);
    // Jump into init process.
    scheduler_enter_user_jmp(
        // Entry point.
        init_p->thread.regs.eip,
        // Stack pointer.
        init_p->thread.regs.useresp);
    // Enable interrupt requests.
    sti();
    for (;;) {}
    // We should not be here.
    pr_emerg("Dear developer, we have to talk...\n");
    return 1;
}
