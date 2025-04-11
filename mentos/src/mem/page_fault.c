/// @file page_fault.c
/// @brief
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PG_FLT]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "mem/page_fault.h"

#include "descriptor_tables/isr.h"
#include "mem/mm/page.h"
#include "mem/mm/vm_area.h"
#include "mem/mm/vmem.h"
#include "process/scheduler.h"
#include "string.h"
#include "system/panic.h"

// Error code interpretation.
#define ERR_PRESENT  0x01 ///< Page not present.
#define ERR_RW       0x02 ///< Page is read only.
#define ERR_USER     0x04 ///< Page is privileged.
#define ERR_RESERVED 0x08 ///< Overwrote reserved bit.
#define ERR_INST     0x10 ///< Instruction fetch.

/// @brief Sets the given page table flags.
/// @param table the page table.
/// @param flags the flags to set.
static inline void __set_pg_table_flags(page_table_entry_t *table, uint32_t flags)
{
    // Check if the table pointer is valid.
    if (!table) {
        pr_crit("Invalid page table entry provided.\n");
        return; // Exit the function early if the table is null.
    }
    // Set the Read/Write flag: 1 if the MM_RW flag is set, 0 otherwise.
    table->rw         = (flags & MM_RW) != 0;
    // Set the Present flag: 1 if the MM_PRESENT flag is set, 0 otherwise.
    table->present    = (flags & MM_PRESENT) != 0;
    // Set the Copy-On-Write flag: 1 if the MM_COW flag is set, 0 otherwise.
    // This flag is used to track if the page is a copy-on-write page.
    table->kernel_cow = (flags & MM_COW) != 0;
    // Set the Available bits: these are reserved for future use, so set them to 1.
    table->available  = 1; // Currently just sets this to 1 as a placeholder.
    // Set the Global flag: 1 if the MM_GLOBAL flag is set, 0 otherwise.
    table->global     = (flags & MM_GLOBAL) != 0;
    // Set the User flag: 1 if the MM_USER flag is set, 0 otherwise.
    table->user       = (flags & MM_USER) != 0;
}

/// @brief Prints stack frame data and calls kernel_panic.
/// @param f    The interrupt stack frame.
/// @param addr The faulting address.
static void __page_fault_panic(pt_regs_t *f, uint32_t addr)
{
    __asm__ __volatile__("cli");

    // Gather fault info and print to screen
    pr_err("Faulting address (cr2): 0x%p\n", addr);

    pr_err("EIP: 0x%p\n", f->eip);

    pr_err("Page fault: 0x%x\n", addr);

    pr_err("Possible causes: [ ");
    if (!(f->err_code & ERR_PRESENT)) {
        pr_err("Page not present ");
    }
    if (f->err_code & ERR_RW) {
        pr_err("Page is read only ");
    }
    if (f->err_code & ERR_USER) {
        pr_err("Page is privileged ");
    }
    if (f->err_code & ERR_RESERVED) {
        pr_err("Overwrote reserved bits ");
    }
    if (f->err_code & ERR_INST) {
        pr_err("Instruction fetch ");
    }
    pr_err("]\n");
    PRINT_REGS(pr_err, f);

    kernel_panic("Page fault!");

    // Make directory accessible
    //    main_mm->pgd->entries[addr/(1024*4096)].user = 1;
    //    main_directory->entries[addr/(1024*4096)]. = 1;

    __asm__ __volatile__("cli");
}

/// @brief Handles the Copy-On-Write (COW) mechanism for a page table entry.
///        If the page is marked as COW, it allocates a new page and updates the entry.
/// @param entry The page table entry to manage.
/// @return 0 on success, 1 on error.
static int __page_handle_cow(page_table_entry_t *entry)
{
    // Check if the entry pointer is valid.
    if (!entry) {
        pr_crit("Invalid page table entry provided.\n");
        return 1;
    }

    // Check if the page is Copy On Write (COW).
    if (entry->kernel_cow) {
        // Mark the page as no longer Copy-On-Write.
        entry->kernel_cow = 0;

        // If the page is not currently present (not allocated in physical memory).
        if (!entry->present) {
            // Allocate a new physical page using high user memory flag.
            page_t *page = alloc_pages(GFP_HIGHUSER, 0);
            if (!page) {
                pr_crit("Failed to allocate a new page.\n");
                return 1;
            }

            // Map the allocated physical page to a virtual address.
            uint32_t vaddr = vmem_map_physical_pages(page, 1);
            if (!vaddr) {
                pr_crit("Failed to map the physical page to virtual address.\n");
                return 1;
            }

            // Clear the new page by setting all its bytes to 0.
            memset((void *)vaddr, 0, PAGE_SIZE);

            // Unmap the virtual address after clearing the page.
            vmem_unmap_virtual_address(vaddr);

            // Set the physical frame address of the allocated page into the entry.
            entry->frame = get_physical_address_from_page(page) >> 12U; // Shift to get page frame number.

            // Mark the page as present in memory.
            entry->present = 1;

            // Success, COW handled and page allocated.
            return 0;
        }
    }

    // If the page is not marked as COW, print an error.
    pr_err("Page not marked as copy-on-write (COW)!\n");

    // Return error as the page is not COW.
    return 1;
}

int init_page_fault(void)
{
    // Install the page fault interrupt service routine (ISR) handler.
    if (isr_install_handler(PAGE_FAULT, page_fault_handler, "page_fault_handler") < 0) {
        pr_crit("Failed to install page fault handler.\n");
        return -1;
    }
    return 0;
}

void page_fault_handler(pt_regs_t *f)
{
    // Here you will find the `Demand Paging` mechanism.
    // From `Understanding The Linux Kernel 3rd Edition`: The term demand paging denotes a dynamic memory allocation
    // technique that consists of deferring page frame allocation until the last possible moment—until the process
    // attempts to address a page that is not present in RAM, thus causing a Page Fault exception.
    //
    // So, if you go inside `mentos/src/exceptions.S`, and check out the macro ISR_ERR, we are pushing the error code on
    // the stack before firing a page fault exception. The error code must be analyzed by the exception handler to
    // determine how to handle the exception. The following bits are the only ones used, all others are reserved.
    // | US RW  P | Description
    // |  0  0  0 | Supervisory process tried to read a non-present page entry
    // |  0  0  1 | Supervisory process tried to read a page and caused a protection fault
    // |  0  1  0 | Supervisory process tried to write to a non-present page entry
    // |  0  1  1 | Supervisory process tried to write a page and caused a protection fault
    // |  1  0  0 | User process tried to read a non-present page entry
    // |  1  0  1 | User process tried to read a page and caused a protection fault
    // |  1  1  0 | User process tried to write to a non-present page entry
    // |  1  1  1 | User process tried to write a page and caused a protection fault

    // Extract the error
    int err_user    = bit_check(f->err_code, 2) != 0;
    int err_rw      = bit_check(f->err_code, 1) != 0;
    int err_present = bit_check(f->err_code, 0) != 0;

    // Extract the address that caused the page fault from the CR2 register.
    uint32_t faulting_addr = get_cr2();

    // Retrieve the current page directory's physical address.
    uint32_t phy_dir = (uint32_t)paging_get_current_pgd();
    if (!phy_dir) {
        pr_crit("Failed to retrieve current page directory.\n");
        __page_fault_panic(f, faulting_addr);
    }

    // Get the page from the physical address of the directory.
    page_t *dir_page = get_page_from_physical_address(phy_dir);
    if (!dir_page) {
        pr_crit("Failed to get page from physical address: %p\n", (void *)phy_dir);
        __page_fault_panic(f, faulting_addr);
    }

    // Get the low memory address from the page and cast it to a page directory structure.
    page_directory_t *lowmem_dir = (page_directory_t *)get_virtual_address_from_page(dir_page);
    if (!lowmem_dir) {
        pr_crit("Failed to get low memory address from page: %p\n", (void *)dir_page);
        __page_fault_panic(f, faulting_addr);
    }

    // Get the directory entry that corresponds to the faulting address.
    page_dir_entry_t *direntry = &lowmem_dir->entries[faulting_addr / (1024U * PAGE_SIZE)];

    // Panic only if page is in kernel memory, else abort process with SIGSEGV.
    if (!direntry->present) {
        pr_crit("ERR(0): Page directory entry not present (%d%d%d)\n", err_user, err_rw, err_present);

        // If the fault was caused by a user process, send a SIGSEGV signal.
        if (err_user) {
            task_struct *task = scheduler_get_current_process();
            if (task) {
                // Notifies current process.
                sys_kill(task->pid, SIGSEGV);
                // Now, we know the process needs to be removed from the list of
                // running processes. We pushed the SEGV signal in the queues of
                // signal to send to the process. To properly handle the signal,
                // just run scheduler.
                scheduler_run(f);
                return;
            }
        }
        pr_crit("ERR(0): So, it is not present, and it was not the user.\n");
        __page_fault_panic(f, faulting_addr);
    }

    // Retrieve the physical address of the page table.
    uint32_t phy_table = direntry->frame << 12U;

    // Get the page from the physical address of the page table.
    page_t *table_page = get_page_from_physical_address(phy_table);
    if (!table_page) {
        pr_crit("Failed to get page from physical address: %p\n", (void *)phy_table);
        __page_fault_panic(f, faulting_addr);
    }

    // Get the low memory address from the page and cast it to a page table structure.
    page_table_t *lowmem_table = (page_table_t *)get_virtual_address_from_page(table_page);
    if (!lowmem_table) {
        pr_crit("Failed to get low memory address from page: %p\n", (void *)table_page);
        __page_fault_panic(f, faulting_addr);
    }

    // Get the entry inside the table that caused the fault.
    uint32_t table_index = (faulting_addr / PAGE_SIZE) % 1024U;

    // Get the corresponding page table entry.
    page_table_entry_t *entry = &lowmem_table->pages[table_index];
    if (!entry) {
        pr_crit("Failed to retrieve page table entry.\n");
        __page_fault_panic(f, faulting_addr);
    }

    // There was a page fault on a virtual mapped address, so we must first
    // update the original mapped page
    if (is_valid_virtual_address(faulting_addr)) {
        // Get the original page table entry from the virtually mapped one.
        page_table_entry_t *orig_entry = (page_table_entry_t *)(*(uint32_t *)entry);
        if (!orig_entry) {
            pr_crit("Original page table entry is NULL.\n");
            __page_fault_panic(f, faulting_addr);
        }

        // Check if the page is Copy on Write (CoW).
        if (__page_handle_cow(orig_entry)) {
            pr_crit("ERR(1): %d%d%d\n", err_user, err_rw, err_present);
            __page_fault_panic(f, faulting_addr);
        }

        // Update the page table entry frame.
        entry->frame = orig_entry->frame;

        // Update the entry flags.
        __set_pg_table_flags(entry, MM_PRESENT | MM_RW | MM_GLOBAL | MM_COW | MM_UPDADDR);
    } else {
        // Check if the page is Copy on Write (CoW).
        if (__page_handle_cow(entry)) {
            pr_crit(
                "Page fault caused by Copy on Write (CoW). Flags: user=%d, "
                "rw=%d, present=%d\n",
                err_user, err_rw, err_present);
            // If the fault was caused by a user process, send a SIGSEGV signal.
            if (err_user && err_rw && err_present) {
                // Get the current process.
                task_struct *task = scheduler_get_current_process();
                if (task) {
                    // Notifies current process.
                    sys_kill(task->pid, SIGSEGV);
                    // Now, we know the process needs to be removed from the list of
                    // running processes. We pushed the SEGV signal in the queues of
                    // signal to send to the process. To properly handle the signal,
                    // just run scheduler.
                    scheduler_run(f);
                    return;
                }
                pr_crit("No task found for current process, unable to send "
                        "SIGSEGV.\n");
            } else {
                pr_crit("Invalid flags for CoW handling, continuing...\n");
            }
            pr_crit("Continuing with page fault handling, triggering panic.\n");
            __page_fault_panic(f, faulting_addr);
        }
    }

    // Invalidate the TLB entry for the faulting address.
    paging_flush_tlb_single(faulting_addr);
}
