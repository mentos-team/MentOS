///                MentOS, The Mentoring Operating system project
/// @file   kernel.c
/// @brief  Kernel main function.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "kernel.h"
#include "zone_allocator.h"
#include "gdt.h"
#include "syscall.h"
#include "version.h"
#include "video.h"

#include "pic8259.h"

#include "cmd_cpuid.h"
#include "debug.h"
#include "fdc.h"
#include "initrd.h"
#include "irqflags.h"
#include "keyboard.h"
#include "scheduler.h"
#include "shell.h"
#include "stdio.h"
#include "timer.h"
#include "vfs.h"

#include "kheap.h"

#include "init.h"

#include "pci.h"
#include "fpu.h"
#include "ata.h"

#include "printk.h"

/// Describe start and end address of grub multiboot modules
char *module_start[MAX_MODULES];
char *module_end[MAX_MODULES];

/// Defined in kernel.ld, points at the multiheader grub info.
extern uint32_t _multiboot_header_start;
extern uint32_t _multiboot_header_end;
/// Defined in kernel.ld, points at the kernel code.
extern uint32_t _text_start;
extern uint32_t _text_end;
/// Defined in kernel.ld, points at the read-only kernel data.
extern uint32_t _rodata_start;
extern uint32_t _rodata_end;
/// Defined in kernel.ld, points at the read-write kernel data initialized.
extern uint32_t _data_start;
extern uint32_t _data_end;
/// Defined in kernel.ld, points at the read-write kernel data uninitialized an kernel stack.
extern uint32_t _bss_start;
extern uint32_t _bss_end;
extern uint32_t stack_top;
extern uint32_t stack_bottom;
/// Defined in kernel.ld, points at the end of kernel code/data.
extern uint32_t end;

uintptr_t initial_esp = 0;

int kmain(uint32_t magic, multiboot_info_t *header, uintptr_t esp)
{
	dbg_print("\n");
	dbg_print("--------------------------------------------------\n");
	dbg_print("- Booting...\n");
	dbg_print("--------------------------------------------------\n");

	// Print kernel initial state
	dbg_print("\n  KERNEL STATE ON BOOTING");
	dbg_print("\nKernel multiboot header:   [ 0x%p - 0x%p ]               ",
			  &_multiboot_header_start, &_multiboot_header_end);
	dbg_print("\nKernel code:               [ 0x%p - 0x%p ] <- text       ",
			  &_text_start, &_text_end);
	dbg_print("\nKernel read-only data:     [ 0x%p - 0x%p ] <- rodata     ",
			  &_rodata_start, &_rodata_end);
	dbg_print("\nKernel initialized data:   [ 0x%p - 0x%p ] <- data       ",
			  &_data_start, &_data_end);
	dbg_print("\nKernel uninitialized data: [ 0x%p - 0x%p ] <- stack & bss",
			  &_bss_start, &_bss_end);
	dbg_print("\n   - Kernel stack:         [ 0x%p - 0x%p ]               ",
			  &stack_bottom, &stack_top);
	dbg_print("\n   - Kernel bss:           [ 0x%p - 0x%p ]               ",
			  &stack_top, &_bss_end);
	dbg_print("\nKernel image end:            0x%p", &end);

	// Am I booted by a Multiboot-compliant boot loader?
	if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
		printk("Invalid magic number: 0x%x\n", (unsigned)magic);
		return 1;
	}

	// Set the initial esp.
	initial_esp = esp;

	// Dump the multiboot structure.
	dump_multiboot(header);

	//=== Initialize the video =================================================
	video_init();
	//--------------------------------------------------------------------------

	//==== Show Operating System Version =======================================
	video_set_color(BRIGHT_GREEN);
	printk(OS_NAME " " OS_VERSION);
	video_set_color(WHITE);
	printk("\nSite:");
	video_set_color(BRIGHT_BLUE);
	printk(OS_SITEURL);
	printk("\n");
	video_set_color(WHITE);
	printk("\n");
	//--------------------------------------------------------------------------

	//==== Set the starting point and end point of the module ==================
	int i = 0;
	multiboot_module_t *mod = (multiboot_module_t *)header->mods_addr;
	for (; i < header->mods_count && i < MAX_MODULES; i++, mod++) {
		module_start[i] = (char *)mod->mod_start;
		module_end[i] = (char *)mod->mod_end;
	}
	//==========================================================================

	//==== Initialize memory ===================================================
	printk("Initialize physical memory manager...");
	// Memoria fisica totale allocata: 1096 MB
	if (!pmmngr_init(1096 * M + 512 * 4 * K)) {
		video_print_fail();
		return 1;
	}
	video_print_ok();
	//==========================================================================

	//==== Initialize kernel heap ==============================================
	dbg_print("Initialize kernel heap.\n");
	printk("Initialize heap...");
	kheap_init(KHEAP_INITIAL_SIZE);
	video_print_ok();
	//==========================================================================

	//==== Initialize core modules =============================================
	// The Global Descriptor Table (GDT) is a data structure used by Intel
	// x86-family processors starting with the 80286 in order to define the
	// characteristics of the various memory areas used during program execution,
	// including the base address, the size, and access privileges like
	// executability and writability. These memory areas are called segments in
	// Intel terminology.
	printk("Initialize GDT...");
	init_gdt();
	video_print_ok();

	// The IDT is used to show the processor what Interrupt Service Routine
	// (ISR) to call to handle an exception. IDT entries are also called
	// Interrupt requests whenever a device has completed a request and needs to
	// be serviced.
	// ISRs are used to save the current processor state and set up the
	// appropriate segment registers needed for kernel mode before the kernel’s
	// C-level interrupt handler is called. To handle the right exception, the
	// correct entry in the IDT should be pointed to the correct ISR.
	printk("Initialize IDT...");
	init_idt();
	video_print_ok();
	//--------------------------------------------------------------------------

	// Initialize the system calls.
	printk("Initialize system calls...");
	syscall_init();
	video_print_ok();

	// Set the IRQs.
	printk("Initialize IRQ...");
	pic8259_init_irq();
	video_print_ok();

	//dbg_print("Initialize the paging.\n");
	// Initialize the paging.
	//printk("Initialize the paging...");
	//paging_init();
	//video_print_ok();

	dbg_print("Install the timer.\n");
	// Install the timer.
	printk(" * Setting up timer...");
	timer_install();
	video_print_ok();

	//dbg_print("Alloc and fill CPUID structure.\n");
	// Alloc and fill CPUID structure.
	//printk(LNG_INIT_CPUID);
	//get_cpuid(&sinfo);
	//video_print_ok();

	dbg_print("Initialize the filesystem.\n");
	// Initialize the filesystem.
	printk("Initialize the fylesystem...");
	vfs_init();
	video_print_ok();
	initfs_init();

	//dbg_print("Get the kernel image from the boot info.\n");
	// Get the kernel image from the boot info
	//build_elf_symbols_from_multiboot(header);

	//==== Install/Uninstall devices ===========================================
	// For debugging, show the list of PCI devices.
	// pci_debug_scan();
	// Scan for ata devices.
	// ata_initialize();

	dbg_print("Install the keyboard.\n");
	printk(" * Setting up keyboard driver...");
	keyboard_install();
	video_print_ok();

	// dbg_print("Install the mouse.\n");
	// printf(" * Setting up mouse driver...");
	// mouse_install();     // Install the mouse.
	// video_print_ok();

	dbg_print("Uninstall the floppy driver.\n");
	fdc_disable_motor(); // We disable floppy driver motor
	//--------------------------------------------------------------------------

	//==== Initialize the scheduler ============================================
	dbg_print("Initialize the scheduler.\n");
	printk("Initialize the scheduler...");
	kernel_initialize_scheduler();
	video_print_ok();
	//--------------------------------------------------------------------------

	// create init process
	task_struct *init_p = create_init_process();

	//==== Initialize the FPU =================================================
	// Initialize the Floating Point Unit (FPU).
	dbg_print("Initialize the Floating Point Unit (FPU).\n");
	printk("Initialize floating point unit...");
	if (!fpu_install()) {
		video_print_fail();
		return 1;
	}
	video_print_ok();

	dbg_print("--------------------------------------------------\n");
	dbg_print("- Booting Done\n");
	dbg_print("- Jumping into init process, hopefully...\n");
	dbg_print("--------------------------------------------------\n");

	// Jump into init process.
	enter_user_jmp(
		// Entry point.
		init_p->thread.eip,
		// Stack pointer.
		init_p->thread.useresp);

	dbg_print("Dear developer, we have to talk...\n");

	return 1;
}
