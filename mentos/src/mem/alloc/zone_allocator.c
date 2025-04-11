/// @file zone_allocator.c
/// @brief Implementation of the Zone Allocator
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[PMM   ]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#include "assert.h"
#include "kernel.h"
#include "list_head.h"
#include "mem/alloc/buddy_system.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/mm/page.h"
#include "mem/paging.h"
#include "string.h"

/// @brief Aligns the given address down to the nearest page boundary.
/// @param addr The address to align.
/// @return The aligned address.
#define MIN_PAGE_ALIGN(addr) ((addr) & (~(PAGE_SIZE - 1)))

/// @brief Aligns the given address up to the nearest page boundary.
/// @param addr The address to align.
/// @return The aligned address.
#define MAX_PAGE_ALIGN(addr) (((addr) & (~(PAGE_SIZE - 1))) + PAGE_SIZE)

/// @brief Aligns the given address down to the nearest order boundary.
/// @param addr The address to align.
/// @return The aligned address.
#define MIN_ORDER_ALIGN(addr) ((addr) & (~((PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)) - 1)))

/// @brief Aligns the given address up to the nearest order boundary.
/// @param addr The address to align.
/// @return The aligned address.
#define MAX_ORDER_ALIGN(addr)                                                                                          \
    (((addr) & (~((PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)) - 1))) +                                              \
     (PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)))

/// @brief Keeps track of system memory management data.
memory_info_t memory;

/// @brief Prints the details of a memory zone.
/// @param log_level the log level.
/// @param name The name of the memory zone (e.g., "LowMem", "HighMem").
/// @param mem_zone Pointer to the memory zone structure.
static inline void __print_memory_zone(int log_level, const char *name, const memory_zone_t *mem_zone)
{
    if (!name) {
        pr_crit("Invalid memory zone name.\n");
        return;
    }
    if (!mem_zone) {
        pr_crit("Invalid memory zone pointer for %s.\n", name);
        return;
    }

    pr_log(log_level, "    %s Zone:\n", name);
    pr_log(log_level, "        Physical : 0x%08x to 0x%08x\n", mem_zone->start_addr, mem_zone->end_addr);
    pr_log(log_level, "        Virtual  : 0x%08x to 0x%08x\n", mem_zone->virt_start, mem_zone->virt_end);
    pr_log(log_level, "        Size     : %s\n", to_human_size(mem_zone->size));
}

/// @brief Prints the details of the system memory management information.
/// @param log_level the log level.
/// @param mem_info Pointer to the memory information structure.
static inline void __print_memory_info(int log_level, const memory_info_t *mem_info)
{
    if (!mem_info) {
        pr_crit("Invalid memory information pointer.\n");
        return;
    }

    pr_log(log_level, "System Memory Information:\n");
    pr_log(log_level, "    Total Memory Size       : %s\n", to_human_size(mem_info->mem_size));
    pr_log(
        log_level, "    Page Indices            : Min: %u, Max: %u\n", mem_info->page_index_min,
        mem_info->page_index_max);
    pr_log(log_level, "Memory Map:\n");
    pr_log(log_level, "    Total Page Frames       : %u\n", mem_info->mem_map_num);
    pr_log(log_level, "    Size                    : %s\n", to_human_size(sizeof(page_t) * mem_info->mem_map_num));
    pr_log(log_level, "Memory Zones:\n");
    __print_memory_zone(log_level, "LowMem", &mem_info->low_mem);
    __print_memory_zone(log_level, "HighMem", &mem_info->high_mem);
}

/// @brief Prints the details of a memory zone.
/// @param log_level the log level.
/// @param zone Pointer to the zone structure to be printed.
static inline void __print_zone(int log_level, const zone_t *zone)
{
    if (!zone) {
        pr_crit("Invalid zone pointer\n");
        return;
    }
    char buddy_status[512] = {0};
    buddy_system_to_string(&zone->buddy_system, buddy_status, sizeof(buddy_status));
    pr_log(log_level, "Zone: %s\n", zone->name);
    pr_log(log_level, "    Number of Pages      : %lu\n", zone->num_pages);
    pr_log(log_level, "    Number of Free Pages : %lu\n", zone->free_pages);
    pr_log(log_level, "    Startint PFN         : %u\n", zone->zone_start_pfn);
    pr_log(log_level, "    Zone Size            : %s\n", to_human_size(zone->total_size));
    pr_log(log_level, "    Zone Memory Map      : 0x%p\n", (uintptr_t)zone->zone_mem_map);
    pr_log(log_level, "    Buddy System Status  : %s\n", buddy_status);
}

/// @brief Get the zone that contains a page frame.
/// @param page A pointer to the page descriptor.
/// @return A pointer to the zone containing the page, or NULL if the page is
/// not within any zone.
static zone_t *get_zone_from_page(page_t *page)
{
    // Validate the input parameter.
    if (!page) {
        pr_crit("Invalid input: page is NULL.\n");
        return NULL;
    }

    // Iterate over all the zones in the contiguous page data structure.
    for (int zone_index = 0; zone_index < memory.page_data->nr_zones; zone_index++) {
        // Get the zone at the given index.
        zone_t *zone = &memory.page_data->node_zones[zone_index];

        // Get the first and last page of the zone. You might be inclide to
        // multiply by the sizeof(page_t), but that is wrong.
        // Considering that it's a sum to a `page_t *`, the arithmetic of
        // pointers takes care of moving the pointer by sizeof(page_t)
        // multiplied by the number of pages.
        page_t *first_page = zone->zone_mem_map;
        page_t *last_page  = zone->zone_mem_map + zone->num_pages;

        // Return the zone if the page is within its range.
        if ((page >= first_page) && (page < last_page)) {
            return zone;
        }
    }

    pr_crit("page is over memory size or not part of any zone.");

    // If no zone contains the page, return NULL.
    return NULL;
}

/// @brief Get a zone from the specified GFP mask.
/// @param gfp_mask GFP flags indicating the type of memory allocation request.
/// @return A pointer to the requested zone, or NULL if the gfp_mask is not
/// recognized.
static zone_t *get_zone_from_flags(gfp_t gfp_mask)
{
    // Ensure that page_data is initialized and valid.
    if (!memory.page_data) {
        pr_crit("page_data is NULL.\n");
        return NULL;
    }

    // Determine the appropriate zone based on the given GFP mask.
    switch (gfp_mask) {
    case GFP_KERNEL:
    case GFP_ATOMIC:
    case GFP_NOFS:
    case GFP_NOIO:
    case GFP_NOWAIT:
        // Return the normal memory zone.
        return &memory.page_data->node_zones[ZONE_NORMAL];

    case GFP_HIGHUSER:
        // Return the high memory zone.
        return &memory.page_data->node_zones[ZONE_HIGHMEM];

    default:
        // If the gfp_mask does not match any recognized flags, log an error and return NULL.
        pr_crit("Unrecognized gfp_mask: %u.\n", gfp_mask);
        return NULL;
    }
}

/// @brief Checks if the specified memory zone is clean (i.e., all pages are free).
/// @param gfp_mask GFP flags indicating the type of memory.
/// @return 1 if the memory is clean (all pages are free), 0 if there is an error or the memory is not clean.
static inline int is_memory_clean(gfp_t gfp_mask)
{
    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);
    // Ensure the zone is valid.
    if (!zone) {
        pr_emerg("Failed to get zone from GFP mask.\n");
        return 0;
    }
    // Check if the total size of the zone matches the free space in the buddy system.
    unsigned long free_space = buddy_system_get_free_space(&zone->buddy_system);
    if (zone->total_size != free_space) {
        pr_crit("Memory zone check failed for zone '%s'.\n", zone->name);
        pr_crit("Expected free space %lu bytes, but found %lu bytes.\n", zone->total_size, free_space);
        pr_crit("Buddy system state for zone '%s':\n", zone->name);
        char buddy_status[512] = {0};
        buddy_system_to_string(&zone->buddy_system, buddy_status, sizeof(buddy_status));
        pr_crit("    %s\n", buddy_status);
        return 0;
    }
    return 1;
}

/// @brief Checks if the physical memory manager is working properly.
/// @return 1 on success, 0 on failure.
static int pmm_check(void)
{
    zone_t *zone_normal = get_zone_from_flags(GFP_KERNEL);
    if (!zone_normal) {
        pr_crit("Failed to retrieve the zone_normal.\n");
        return 0;
    }
    zone_t *zone_highmem = get_zone_from_flags(GFP_HIGHUSER);
    if (!zone_highmem) {
        pr_crit("Failed to retrieve the zone_highmem.\n");
        return 0;
    }
    // Verify memory state.
    if (!is_memory_clean(GFP_KERNEL)) {
        pr_err("Memory not clean.\n");
        return 0;
    }
    if (!is_memory_clean(GFP_HIGHUSER)) {
        pr_err("Memory not clean.\n");
        return 0;
    }

    char buddy_status[512] = {0};
    pr_notice("Zones status before testing:\n");
    buddy_system_to_string(&zone_normal->buddy_system, buddy_status, sizeof(buddy_status));
    pr_notice("    %s\n", buddy_status);
    buddy_system_to_string(&zone_highmem->buddy_system, buddy_status, sizeof(buddy_status));
    pr_notice("    %s\n", buddy_status);

    pr_notice("\tStep 1: Testing allocation in kernel-space...\n");
    {
        // Allocate a single page with GFP_KERNEL.
        page_t *page = alloc_pages(GFP_KERNEL, 0);
        if (!page) {
            pr_err("Page allocation failed.\n");
            return 0;
        }
        // Free the allocated page.
        if (free_pages(page) < 0) {
            pr_err("Page deallocation failed.\n");
            return 0;
        }
        // Verify memory state after deallocation.
        if (!is_memory_clean(GFP_KERNEL)) {
            pr_err("Test failed: Memory not clean.\n");
            return 0;
        }
    }
    pr_notice("\tStep 2: Testing allocation in user-space...\n");
    {
        // Allocate a single page with GFP_HIGHUSER.
        page_t *page = alloc_pages(GFP_HIGHUSER, 0);
        if (!page) {
            pr_err("Page allocation failed.\n");
            return 0;
        }
        // Free the allocated page.
        if (free_pages(page) < 0) {
            pr_err("Page deallocation failed.\n");
            return 0;
        }
        // Verify memory state after deallocation.
        if (!is_memory_clean(GFP_HIGHUSER)) {
            pr_err("Test failed: Memory not clean.\n");
            return 0;
        }
    }
    pr_notice("\tStep 3: Testing allocation of five 2^{i} page frames in "
              "user-space...\n");
    {
        page_t *pages[5];
        // Allocate pages with GFP_HIGHUSER.
        for (int i = 0; i < 5; i++) {
            pages[i] = alloc_pages(GFP_HIGHUSER, i);
            if (!pages[i]) {
                pr_err("Page allocation failed.\n");
                return 0;
            }
        }
        // Free the allocated pages.
        for (int i = 0; i < 5; i++) {
            if (free_pages(pages[i]) < 0) {
                pr_err("Page deallocation failed.\n");
                return 0;
            }
        }
        // Verify memory state after deallocation.
        if (!is_memory_clean(GFP_HIGHUSER)) {
            pr_err("Test failed: Memory not clean.\n");
            return 0;
        }
    }
    pr_notice("\tStep 4: Testing allocation of five 2^{i} page frames in "
              "kernel-space...\n");
    {
        page_t *pages[5];
        // Allocate pages with GFP_KERNEL.
        for (int i = 0; i < 5; i++) {
            pages[i] = alloc_pages(GFP_KERNEL, i);
            if (!pages[i]) {
                pr_err("Page allocation failed.\n");
                return 0;
            }
        }
        // Free the allocated pages.
        for (int i = 0; i < 5; i++) {
            if (free_pages(pages[i]) < 0) {
                pr_err("Page deallocation failed.\n");
                return 0;
            }
        }
        // Verify memory state after deallocation.
        if (!is_memory_clean(GFP_KERNEL)) {
            pr_err("Test failed: Memory not clean.\n");
            return 0;
        }
    }
    return 1;
}

/// @brief Initializes the memory attributes for a specified zone.
/// @param name The zone's name.
/// @param zone_index The zone's index, which must be valid within the number of zones.
/// @param adr_from The lowest address of the zone (inclusive).
/// @param adr_to The highest address of the zone (exclusive).
/// @return 1 on success, 0 on error.
static int zone_init(char *name, int zone_index, uint32_t adr_from, uint32_t adr_to)
{
    // Ensure that the provided addresses are valid: adr_from must be less than adr_to.
    if (adr_from >= adr_to) {
        pr_crit(
            "Invalid block addresses: adr_from (%u) must be less than adr_to "
            "(%u).\n",
            adr_from, adr_to);
        return 0;
    }

    // Ensure that adr_from is page-aligned.
    if ((adr_from & 0xfffff000) != adr_from) {
        pr_crit("adr_from (%u) must be page-aligned.\n", adr_from);
        return 0;
    }

    // Ensure that adr_to is page-aligned.
    if ((adr_to & 0xfffff000) != adr_to) {
        pr_crit("adr_to (%u) must be page-aligned.\n", adr_to);
        return 0;
    }

    // Ensure that the zone_index is within the valid range.
    if ((zone_index < 0) || (zone_index >= memory.page_data->nr_zones)) {
        pr_crit("The zone_index (%d) is out of bounds (max: %d).\n", zone_index, memory.page_data->nr_zones - 1);
        return 0;
    }

    // Take the zone_t structure that corresponds to the zone_index.
    zone_t *zone = &memory.page_data->node_zones[zone_index];

    // Ensure that the zone was retrieved successfully.
    if (!zone) {
        pr_crit("Failed to retrieve the zone for zone_index: %d.\n", zone_index);
        return 0;
    }

    // Calculate the number of page frames in the zone.
    size_t num_page_frames = (adr_to - adr_from) / PAGE_SIZE;

    // Calculate the index of the first page frame of the zone.
    uint32_t first_page_frame = adr_from / PAGE_SIZE;

    // Update zone information.
    zone->name           = name;                              // Set the zone's name.
    zone->num_pages      = num_page_frames;                   // Set the total number of page frames.
    zone->free_pages     = num_page_frames;                   // Initialize free pages to the total number.
    zone->zone_mem_map   = memory.mem_map + first_page_frame; // Map the memory for the zone.
    zone->zone_start_pfn = first_page_frame;                  // Set the starting page frame number.
    zone->total_size     = adr_to - adr_from;                 // Save the total size of the zone in bytes.

    // Clear the page structures in the memory map.
    memset(zone->zone_mem_map, 0, zone->num_pages * sizeof(page_t));

    // Initialize the buddy system for the new zone.
    if (!buddy_system_init(
            &zone->buddy_system,             // Buddy system structure for the zone.
            name,                            // Name of the zone.
            zone->zone_mem_map,              // Pointer to the memory map of the zone.
            BBSTRUCT_OFFSET(page_t, bbpage), // Offset for the buddy system structure.
            sizeof(page_t),                  // Size of each page.
            num_page_frames                  // Total number of page frames in the zone.
            )) {
        pr_crit("Failed to initialize the buddy system for zone '%s'.\n", name);
        return 0;
    }

    __print_zone(LOGLEVEL_NOTICE, zone);

    return 1;
}

int is_valid_virtual_address(uint32_t addr)
{
    if ((addr >= memory.low_mem.virt_start) && (addr < memory.low_mem.virt_end)) {
        return 1;
    }
    if ((addr >= memory.high_mem.virt_start) && (addr < memory.high_mem.virt_end)) {
        return 1;
    }
    return 0;
}

/*
 * AAAABBBBCCCC
 *    ZZZZZZ
 *
 * */

uint32_t find_nearest_order_greater(uint32_t base_addr, uint32_t amount)
{
    // Calculate the starting page frame number (PFN) based on the base address.
    uint32_t start_pfn = base_addr / PAGE_SIZE;

    // Calculate the ending page frame number (PFN) based on the base address and amount.
    uint32_t end_pfn = (base_addr + amount + PAGE_SIZE - 1) / PAGE_SIZE;

    // Ensure that the number of pages is positive.
    assert(end_pfn > start_pfn && "Calculated number of pages must be greater than zero.");

    // Calculate the number of pages required.
    uint32_t npages = end_pfn - start_pfn;

    // Find the fitting order (power of two) that can accommodate the required
    // number of pages.
    uint32_t order = 0;
    while ((1UL << order) < npages) {
        ++order;
    }

    return order; // Return the calculated order.
}

/// @brief Initializes the array of page_t structures for memory management.
/// @param boot_info Boot information.
/// @param offset Offset from lowmem_start where the pages will be placed.
/// @return int Returns the amount of memory used on success, or -1 on error.
ssize_t pmmngr_initialize_pages(const boot_info_t *boot_info, size_t offset)
{
    // Ensure input pointers are valid.
    if (!boot_info) {
        pr_crit("Invalid input parameters to initialize_pages\n");
        return -1;
    }

    pr_debug("Initializing low memory map structure...\n");

    // Compute the size of memory.
    memory.mem_size = boot_info->highmem_phy_end;
    if (memory.mem_size == 0) {
        pr_crit("Memory size is zero, cannot initialize pages\n");
        return -1;
    }

    // Calculate the total number of pages (all LowMem + HighMem RAM).
    memory.mem_map_num = memory.mem_size / PAGE_SIZE;

    // Set the base address for the page_t array.
    uintptr_t mem_start = boot_info->lowmem_virt_start + offset;
    // Compute the memory usage.
    size_t mem_usage    = sizeof(page_t) * memory.mem_map_num;
    // Get the pointer to where we want to place the structure.
    memory.mem_map      = (page_t *)mem_start;

    // Ensure the memory usage does not exceed LowMem.
    if ((mem_start + mem_usage) > boot_info->lowmem_virt_end) {
        pr_crit("Insufficient LowMem to allocate memory map\n");
        return -1;
    }

    // Initialize each page_t structure.
    for (size_t i = 0; i < memory.mem_map_num; ++i) {
        // Mark page as free by setting its count to 0.
        set_page_count(&memory.mem_map[i], 0);
    }

    pr_debug("Memory map structure initialized with %zu pages\n", memory.mem_map_num);

    return (ssize_t)mem_usage;
}

/// @brief Initializes the contiguous page data node.
/// @param boot_info Boot information.
/// @param offset Virtual memory offset from the base address.
/// @return int Returns the amount of memory used on success, or -1 on error.
ssize_t pmmngr_initialize_page_data(const boot_info_t *boot_info, size_t offset)
{
    // Ensure input pointers are valid.
    if (!boot_info || !memory.mem_map) {
        pr_crit("Invalid input parameters to initialize_contig_page_data\n");
        return -1;
    }

    pr_debug("Initializing page_data node...\n");

    // Calculate the virtual start address for page_data.
    uintptr_t virt_start = boot_info->lowmem_virt_start + offset;
    // Compute the memory usage.
    size_t mem_usage     = sizeof(pg_data_t);
    // Get the pointer to where we want to place the structure.
    memory.page_data     = (pg_data_t *)virt_start;

    // Initialize the page_data fields.
    memory.page_data->nr_zones         = __MAX_NR_ZONES;     // Number of memory zones.
    memory.page_data->node_id          = 0;                  // Node ID (0 for UMA systems).
    memory.page_data->node_mem_map     = memory.mem_map;     // Corresponding to mem_map.
    memory.page_data->node_next        = NULL;               // No next node in UMA.
    memory.page_data->node_size        = memory.mem_map_num; // Total number of pages.
    memory.page_data->node_start_mapnr = 0;                  // Start index in mem_map.
    memory.page_data->node_start_paddr = 0x0;                // Physical address of the first page.

    // Ensure the memory usage does not exceed LowMem.

    if (virt_start + mem_usage > boot_info->lowmem_virt_end) {
        pr_crit("Insufficient LowMem to allocate page_data\n");
        return -1;
    }

    pr_debug("page_data node initialized: %p\n", memory.page_data);

    return (ssize_t)mem_usage;
}

int pmmngr_init(boot_info_t *boot_info)
{
    // Place the pages in memory.
    ssize_t offset_pages = pmmngr_initialize_pages(boot_info, 0U);

    // Place the page data in memory.
    ssize_t offset_page_data = pmmngr_initialize_page_data(boot_info, offset_pages);

    // Compute the physical and virtual start addresses for the LowMem zone
    uint32_t tmp_normal_phy_start  = boot_info->lowmem_phy_start + offset_pages + offset_page_data;
    uint32_t tmp_normal_virt_start = boot_info->lowmem_virt_start + offset_pages + offset_page_data;
    // Align the physical start address of the LowMem zone to the nearest valid boundary.
    memory.low_mem.start_addr      = MAX_PAGE_ALIGN(tmp_normal_phy_start);
    // Align the physical end address of the LowMem zone to the nearest lower valid boundary.
    memory.low_mem.end_addr        = MIN_PAGE_ALIGN(boot_info->lowmem_phy_end);
    // Calculate the size of the LowMem zone, ensuring it is aligned to the buddy system order.
    memory.low_mem.size            = MIN_ORDER_ALIGN(memory.low_mem.end_addr - memory.low_mem.start_addr);
    // Recalculate the end address of the LowMem zone based on the adjusted size.
    memory.low_mem.end_addr        = memory.low_mem.start_addr + memory.low_mem.size;
    // Compute the virtual addresses for LowMem, offset by the adjusted physical start address.
    memory.low_mem.virt_start      = tmp_normal_virt_start + (memory.low_mem.start_addr - tmp_normal_phy_start);
    memory.low_mem.virt_end        = boot_info->lowmem_virt_end;

    // Align the physical start address of the HighMem zone to the nearest valid boundary.
    memory.high_mem.start_addr = MAX_PAGE_ALIGN((uint32_t)boot_info->highmem_phy_start);
    // Align the physical end address of the HighMem zone to the nearest lower valid boundary.
    memory.high_mem.end_addr   = MIN_PAGE_ALIGN((uint32_t)boot_info->highmem_phy_end);
    // Calculate the size of the HighMem zone, ensuring it is aligned to the buddy system order.
    memory.high_mem.size       = MIN_ORDER_ALIGN(memory.high_mem.end_addr - memory.high_mem.start_addr);
    // Recalculate the aligned physical end address of the HighMem zone based on the adjusted size.
    memory.high_mem.end_addr   = memory.high_mem.start_addr + memory.high_mem.size;
    // Compute the virtual addresses for the HighMem zone.
    memory.high_mem.virt_start = memory.low_mem.virt_end;
    memory.high_mem.virt_end   = memory.high_mem.virt_start + memory.high_mem.size;

    // Calculate the minimum page index (start of LowMem).
    memory.page_index_min = memory.low_mem.start_addr / PAGE_SIZE;
    // Calculate the maximum page index (end of HighMem).
    memory.page_index_max = (memory.high_mem.end_addr / PAGE_SIZE) - 1;

    if (!zone_init("Normal", ZONE_NORMAL, memory.low_mem.start_addr, memory.low_mem.end_addr)) {
        return 0;
    }
    if (!zone_init("HighMem", ZONE_HIGHMEM, memory.high_mem.start_addr, memory.high_mem.end_addr)) {
        return 0;
    }

    __print_memory_info(LOGLEVEL_NOTICE, &memory);

    return pmm_check();
}

page_t *pr_alloc_pages(const char *file, const char *func, int line, gfp_t gfp_mask, uint32_t order)
{
    // Calculate the block size based on the order.
    uint32_t block_size = 1UL << order;

    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);

    // Ensure the zone is valid.
    if (!zone) {
        pr_emerg("Failed to get zone from GFP mask.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Allocate a page from the buddy system of the zone.
    bb_page_t *bbpage = bb_alloc_pages(&zone->buddy_system, order);

    // Ensure the allocation was successful.
    if (!bbpage) {
        pr_crit("Failed to allocate page from buddy system.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Convert the buddy system page structure to the page_t structure.
    page_t *page = PG_FROM_BBSTRUCT(bbpage, page_t, bbpage);

    // Ensure the page allocation was successful.
    if (!page) {
        pr_emerg("Page allocation failed.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Set page counters for each page in the block.
    for (uint32_t i = 0; i < block_size; i++) {
        set_page_count(&page[i], 1);
    }

    // Decrement the number of free pages in the zone.
    zone->free_pages -= block_size;

#ifdef ENABLE_PAGE_TRACE
    pr_notice("BS-A: (page: %p order: %d)\n", page, order);
#endif
    // Return the pointer to the first page in the allocated block.
    return page;
}

int pr_free_pages(const char *file, const char *func, int line, page_t *page)
{
    // Get the zone that contains the given page.
    zone_t *zone = get_zone_from_page(page);

    // Ensure the zone retrieval was successful.
    if (!zone) {
        pr_emerg("Failed to get zone from page. Page is over memory size.\n");
        return -1; // Return -1 to indicate failure.
    }

    // Ensure the page is within the selected zone.
    if (zone->zone_mem_map > page) {
        pr_emerg("Page is below the selected zone!\n");
        return -1; // Return -1 to indicate failure.
    }

    // Get the order and block size of the page.
    uint32_t order      = page->bbpage.order;
    uint32_t block_size = 1UL << order;

    // Set page counters to 0 for each page in the block.
    for (uint32_t i = 0; i < block_size; i++) {
        set_page_count(&page[i], 0);
    }

    // Free the pages in the buddy system.
    bb_free_pages(&zone->buddy_system, &page->bbpage);

    // Increment the number of free pages in the zone.
    zone->free_pages += block_size;

#ifdef ENABLE_PAGE_TRACE
    pr_notice("BS-F: (page: %p order: %d)\n", page, order);
#endif

    return 0;
}

uint32_t alloc_pages_lowmem(gfp_t gfp_mask, uint32_t order)
{
    // Ensure the order is within the valid range.
    if (order >= MAX_BUDDYSYSTEM_GFP_ORDER) {
        pr_emerg("Order exceeds the maximum limit.\n");
        return 0; // Return 0 to indicate failure.
    }

    // Ensure the GFP mask is correct.
    if (gfp_mask != GFP_KERNEL) {
        pr_emerg("Invalid GFP mask. Expected GFP_KERNEL.\n");
        return 0; // Return 0 to indicate failure.
    }

    // Allocate the pages based on the given GFP mask and order.
    page_t *page = alloc_pages(gfp_mask, order);

    // Ensure the page allocation was successful.
    if (!page) {
        pr_emerg("Page allocation failed.\n");
        return 0; // Return 0 to indicate failure.
    }

    // Get the low memory address of the first page in the allocated block.
    uint32_t block_frame_adr = get_virtual_address_from_page(page);

    // Ensure the address retrieval was successful.
    if (block_frame_adr == (uint32_t)-1) {
        pr_emerg("Failed to get low memory address from page.\n");
        return 0; // Return 0 to indicate failure.
    }
#if 0
    pr_debug("BS-G: addr: %p (page: %p order: %d)\n", block_frame_adr, page, order);
#endif

    // Return the low memory address of the first page in the allocated block.
    return block_frame_adr;
}

int free_pages_lowmem(uint32_t vaddr)
{
    // Ensure it is a valid virtual address.
    if (!is_valid_virtual_address(vaddr)) {
        pr_crit("The provided address 0x%p is not a valid virtual address.\n", vaddr);
        return -1;
    }

    // Get the page corresponding to the given low memory address.
    page_t *page = get_page_from_virtual_address(vaddr);

    // Ensure the page retrieval was successful.
    if (!page) {
        pr_emerg(
            "Failed to retrieve page from address: 0x%p. Page is over memory "
            "size.\n",
            vaddr);
        return -1;
    }

    // Free the pages starting from the given page.
    free_pages(page);

    return 0;
}

unsigned long get_zone_total_space(gfp_t gfp_mask)
{
    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);

    // Ensure the zone retrieval was successful.
    if (!zone) {
        pr_emerg("Cannot retrieve the correct zone for GFP mask: 0x%x.\n", gfp_mask);
        return 0; // Return 0 to indicate failure.
    }

    // Return the total space of the zone.
    return buddy_system_get_total_space(&zone->buddy_system);
}

unsigned long get_zone_free_space(gfp_t gfp_mask)
{
    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);

    // Ensure the zone retrieval was successful.
    if (!zone) {
        pr_emerg("Cannot retrieve the correct zone for GFP mask: 0x%x.\n", gfp_mask);
        return 0; // Return 0 to indicate failure.
    }

    // Return the free space of the zone.
    return buddy_system_get_free_space(&zone->buddy_system);
}

unsigned long get_zone_cached_space(gfp_t gfp_mask)
{
    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);

    // Ensure the zone retrieval was successful.
    if (!zone) {
        pr_emerg("Cannot retrieve the correct zone for GFP mask: 0x%x.\n", gfp_mask);
        return 0; // Return 0 to indicate failure.
    }

    // Return the cached space of the zone.
    return buddy_system_get_cached_space(&zone->buddy_system);
}

int get_zone_buddy_system_status(gfp_t gfp_mask, char *buffer, size_t bufsize)
{
    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);

    // Ensure the zone retrieval was successful.
    if (!zone) {
        pr_emerg("Cannot retrieve the correct zone for GFP mask: 0x%x.\n", gfp_mask);
        return 0; // Return 0 to indicate failure.
    }

    return buddy_system_to_string(&zone->buddy_system, buffer, bufsize);
}
