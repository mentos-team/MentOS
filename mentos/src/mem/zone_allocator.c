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
#include "mem/buddysystem.h"
#include "mem/paging.h"
#include "mem/zone_allocator.h"
#include "string.h"
#include "list_head.h"

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
#define MAX_ORDER_ALIGN(addr)                                             \
    (((addr) & (~((PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)) - 1))) + \
     (PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)))

/// @brief Array of all physical memory blocks (pages).
/// @details This variable points to an array of `page_t` structures
/// representing all physical memory blocks (pages) in the system. It is used to
/// track the state of each page in memory.
page_t *mem_map = NULL;

/// @brief Memory node descriptor for contiguous memory.
/// @details This variable points to the `pg_data_t` structure, which represents
/// a memory node (usually for NUMA systems). It typically describes the memory
/// properties and zones for a contiguous block of physical memory.
pg_data_t *contig_page_data = NULL;

/// @brief Virtual base address of the low memory (lowmem) zone.
/// @details This variable stores the base virtual address of the low memory
/// region. The kernel uses this address to access low memory (directly
/// addressable memory).
uint32_t lowmem_virt_base = 0;

/// @brief Physical base address of the low memory (lowmem) zone.
/// @details This variable stores the base physical address of the low memory
/// region. It represents the starting point of the lowmem pages in physical
/// memory.
uint32_t lowmem_page_base = 0;

/// @brief Physical start address of low memory (lowmem) zone.
/// @details This variable stores the physical address where the low memory
/// (which is directly addressable by the kernel) begins.
uint32_t lowmem_phy_start;

/// @brief Virtual start address of low memory (lowmem) zone.
/// @details This variable stores the virtual address corresponding to the start
/// of the low memory region in the kernel's virtual address space.
uint32_t lowmem_virt_start;

/// @brief Total size of available physical memory in bytes.
/// @details This variable holds the total amount of memory available on the
/// system (both low and high memory).
uint32_t mem_size;

/// @brief Total number of memory frames (pages) available.
/// @details The number of physical memory frames available in the system,
/// calculated as the total memory divided by the size of a memory page.
uint32_t mem_map_num;

/// @brief Start address of the normal (lowmem) zone.
/// @details This variable holds the starting physical address of the normal
/// memory zone, also known as low memory, which is directly addressable by the
/// kernel.
uint32_t normal_start_addr;

/// @brief End address of the normal (lowmem) zone.
/// @details This variable holds the ending physical address of the normal
/// memory zone (low memory), marking the boundary between lowmem and highmem.
uint32_t normal_end_addr;

/// @brief Total size of the normal (lowmem) zone.
/// @details The size of the normal memory zone in bytes, which is the portion
/// of memory directly addressable by the kernel.
uint32_t normal_size;

/// @brief Start address of the high memory (highmem) zone.
/// @details This variable stores the starting physical address of the high
/// memory zone, which is memory not directly addressable by the kernel and
/// requires special handling.
uint32_t high_start_addr;

/// @brief End address of the high memory (highmem) zone.
/// @details This variable holds the ending physical address of the high memory
/// zone, which marks the limit of available physical memory.
uint32_t high_end_addr;

/// @brief Total size of the high memory (highmem) zone.
/// @details The size of the high memory zone in bytes. High memory requires
/// special handling as it is not directly accessible by the kernel's virtual
/// address space.
uint32_t high_size;

uint32_t get_virtual_address_from_page(page_t *page)
{
    // Check for NULL page pointer. If it is NULL, print an error and return 0.
    if (!page) {
        pr_err("Invalid page pointer: NULL value provided.\n");
        return 0; // Return 0 to indicate an error in retrieving the address.
    }

    // Calculate the index of the page in the memory map.
    unsigned int page_index = page - mem_map;

    // Check if the calculated page index is within valid bounds.
    if ((page_index < lowmem_page_base) || (page_index >= mem_map_num)) {
        pr_err("Page %p at index %u is out of bounds. Valid range: %u to %u.\n",
               (void *)page, page_index, lowmem_page_base, mem_map_num - 1);
        return 0;
    }

    // Calculate the offset from the low memory base address.
    unsigned int offset = page_index - lowmem_page_base;

    // Return the corresponding low memory virtual address.
    return lowmem_virt_base + (offset * PAGE_SIZE);
}

uint32_t get_physical_address_from_page(page_t *page)
{
    // Ensure the page pointer is not NULL. If it is NULL, print an error and return 0.
    if (!page) {
        pr_err("Invalid page pointer: NULL value provided.\n");
        return 0; // Return 0 to indicate an error in retrieving the address.
    }

    // Calculate the index of the page in the memory map.
    unsigned int page_index = page - mem_map;

    // Check if the calculated page index is within valid bounds.
    if ((page_index < lowmem_page_base) || (page_index >= mem_map_num)) {
        pr_err("Page %p at index %u is out of bounds. Valid range: %u to %u.\n",
               (void *)page, page_index, lowmem_page_base, mem_map_num - 1);
        return 0;
    }

    // Return the corresponding physical address by multiplying the index by the
    // page size.
    return page_index * PAGE_SIZE;
}

page_t *get_page_from_virtual_address(uint32_t vaddr)
{
    // Ensure the address is within the valid range.
    if (vaddr < lowmem_virt_base) {
        pr_crit("Address is below low memory virtual base.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Calculate the offset from the low memory virtual base address.
    unsigned int offset = vaddr - lowmem_virt_base;

    // Determine the index of the corresponding page structure in the memory map.
    unsigned int page_index = lowmem_page_base + (offset / PAGE_SIZE);

    // Check if the page index exceeds the memory map limit.
    if (page_index >= mem_map_num) {
        pr_crit("Page index %u is out of bounds. Maximum allowed index is %u.\n",
                page_index, mem_map_num - 1);
        return NULL; // Return NULL to indicate failure.
    }

    // Return the pointer to the page structure.
    return mem_map + page_index;
}

page_t *get_page_from_physical_address(uint32_t phy_addr)
{
    // Ensure the physical address is valid and aligned to page boundaries.
    if (phy_addr % PAGE_SIZE != 0) {
        pr_crit("Address must be page-aligned. Received address: 0x%08x\n", phy_addr);
        return NULL; // Return NULL to indicate failure due to misalignment.
    }

    // Calculate the index of the page in the memory map.
    unsigned int page_index = phy_addr / PAGE_SIZE;

    // Check if the calculated page index is within valid bounds.
    if ((page_index < lowmem_page_base) || (page_index >= mem_map_num)) {
        pr_err("Page index %u is out of bounds. Valid range: %u to %u.\n",
               page_index, lowmem_page_base, mem_map_num - 1);
        return NULL; // Return NULL to indicate failure.
    }

    // Return the pointer to the corresponding page structure in the memory map.
    return mem_map + page_index;
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
        return NULL; // Return NULL to indicate failure due to NULL input.
    }

    // Iterate over all the zones in the contiguous page data structure.
    for (int zone_index = 0; zone_index < contig_page_data->nr_zones; zone_index++) {
        // Get the zone at the given index.
        zone_t *zone = contig_page_data->node_zones + zone_index;

        // Get the first and last page of the zone by adding the zone size to
        // the base of the memory map.
        page_t *first_page = zone->zone_mem_map;
        page_t *last_page  = zone->zone_mem_map + zone->size;

        // Check if the given page is within the current zone.
        if ((page >= first_page) && (page < last_page)) {
            return zone; // Return the zone if the page is within its range.
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
    // Ensure that contig_page_data is initialized and valid.
    if (!contig_page_data) {
        pr_crit("contig_page_data is NULL.\n");
        return NULL; // Return NULL to indicate failure due to uninitialized data.
    }

    // Determine the appropriate zone based on the given GFP mask.
    switch (gfp_mask) {
    case GFP_KERNEL:
    case GFP_ATOMIC:
    case GFP_NOFS:
    case GFP_NOIO:
    case GFP_NOWAIT:
        // Return the normal zone for these GFP flags.
        return &contig_page_data->node_zones[ZONE_NORMAL];

    case GFP_HIGHUSER:
        // Return the high memory zone for GFP_HIGHUSER.
        return &contig_page_data->node_zones[ZONE_HIGHMEM];

    default:
        // If the gfp_mask does not match any recognized flags, log an error and return NULL.
        pr_crit("Unrecognized gfp_mask: %u.\n", gfp_mask);
        return NULL; // Return NULL to indicate that the input was not valid.
    }
}

/// @brief Checks if the specified memory zone is clean (i.e., all pages are free).
/// @param gfp_mask The mask that specifies the zone of interest for memory allocation.
/// @return 1 if the memory is clean, 0 if there is an error or if the memory is not clean.
static int is_memory_clean(gfp_t gfp_mask)
{
    // Get the corresponding zone based on the gfp_mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);
    if (!zone) {
        pr_crit("Failed to retrieve the zone for gfp_mask: %u.\n", gfp_mask);
        return 0; // Return 0 to indicate an error due to invalid zone.
    }

    // Get the last free area list of the buddy system.
    bb_free_area_t *area = zone->buddy_system.free_area + (MAX_BUDDYSYSTEM_GFP_ORDER - 1);
    if (!area) {
        pr_crit("Failed to retrieve the last free_area for the zone.\n");
        return 0; // Return 0 to indicate an error due to invalid area.
    }

    // Compute the total size of the zone.
    unsigned int total_size = (zone->size / (1UL << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)));

    // Check if the size of the zone matches the number of free pages in the area.
    if (area->nr_free != total_size) {
        pr_crit("Number of blocks of free pages is different than expected (%d vs %d).\n", area->nr_free, total_size);

        // Dump the current state of the buddy system for debugging purposes.
        buddy_system_dump(&zone->buddy_system);

        // Return 0 to indicate an error.
        return 0;
    }

    // Return 1 if the memory is clean (i.e., the sizes match).
    return 1;
}

/// @brief  Checks if the physical memory manager is working properly.
/// @return If the check was done correctly.
static int pmm_check(void)
{
    pr_debug(
        "\n=================== ZONE ALLOCATOR TEST ==================== \n");

    pr_debug("\t[STEP1] One page frame in kernel-space... ");
    pr_debug("\n\t ===== [STEP1] One page frame in kernel-space ====\n");
    pr_debug("\n\t ----- ALLOC -------------------------------------\n");
    uint32_t ptr1 = __alloc_page_lowmem(GFP_KERNEL);
    pr_debug("\n\t ----- FREE --------------------------------------\n");
    free_page_lowmem(ptr1);
    if (!is_memory_clean(GFP_KERNEL)) {
        pr_emerg("Test failed, memory is not clean.\n");
        return 0;
    }

    pr_debug("\t[STEP2] Five page frames in user-space... ");
    pr_debug("\n\t ===== [STEP2] Five page frames in user-space ====\n");
    page_t *ptr2[5];
    for (int i = 0; i < 5; i++) {
        ptr2[i] = _alloc_pages(GFP_HIGHUSER, 0);
    }
    for (int i = 0; i < 5; i++) {
        __free_pages(ptr2[i]);
    }
    if (!is_memory_clean(GFP_HIGHUSER)) {
        pr_emerg("Test failed, memory is not clean.\n");
        return 0;
    }

    pr_debug("\t[STEP3] 2^{3} page frames in kernel-space... ");
    pr_debug("\n\t ===== [STEP3] 2^{3} page frames in kernel-space ====\n");
    uint32_t ptr3 = __alloc_pages_lowmem(GFP_KERNEL, 3);
    free_pages_lowmem(ptr3);
    if (!is_memory_clean(GFP_KERNEL)) {
        pr_emerg("Test failed, memory is not clean.\n");
        return 0;
    }

    pr_debug("\t[STEP4] Five 2^{i} page frames in user-space... ");
    pr_debug("\n\t ===== [STEP4] Five 2^{i} page frames in user-space ====\n");
    page_t *ptr4[5];
    for (int i = 0; i < 5; i++) {
        ptr4[i] = _alloc_pages(GFP_HIGHUSER, i);
    }
    for (int i = 0; i < 5; i++) {
        __free_pages(ptr4[i]);
    }
    if (!is_memory_clean(GFP_HIGHUSER)) {
        pr_emerg("Test failed, memory is not clean.\n");
        return 0;
    }

    pr_debug("\t[STEP5] Mixed page frames in kernel-space... ");
    pr_debug("\n\t ===== [STEP5] Mixed page frames in kernel-space ====\n");
    int **ptr = (int **)__alloc_page_lowmem(GFP_KERNEL);
    int i     = 0;
    for (; i < 5; ++i) {
        ptr[i] = (int *)__alloc_page_lowmem(GFP_KERNEL);
    }
    for (; i < 20; ++i) {
        ptr[i] = (int *)__alloc_pages_lowmem(GFP_KERNEL, 2);
    }

    int j = 0;
    for (; j < 5; ++j) {
        free_page_lowmem((uint32_t)ptr[j]);
    }
    for (; j < 20; ++j) {
        free_pages_lowmem((uint32_t)ptr[j]);
    }
    free_page_lowmem(ptr1);

    if (!is_memory_clean(GFP_KERNEL)) {
        pr_emerg("Test failed, memory is not clean.\n");
        return 0;
    }
    return 1;
}

/// @brief Initializes the memory attributes for a specified zone.
/// @param name The zone's name.
/// @param zone_index The zone's index, which must be valid within the number of zones.
/// @param adr_from The lowest address of the zone (inclusive).
/// @param adr_to The highest address of the zone (exclusive).
/// @return 0 on success, -1 on error.
static int zone_init(char *name, int zone_index, uint32_t adr_from, uint32_t adr_to)
{
    // Ensure that the provided addresses are valid: adr_from must be less than adr_to.
    if (adr_from >= adr_to) {
        pr_crit("Invalid block addresses: adr_from (%u) must be less than adr_to (%u).\n", adr_from, adr_to);
        return -1; // Return -1 to indicate an error.
    }

    // Ensure that adr_from is page-aligned.
    if ((adr_from & 0xfffff000) != adr_from) {
        pr_crit("adr_from (%u) must be page-aligned.\n", adr_from);
        return -1; // Return -1 to indicate an error.
    }

    // Ensure that adr_to is page-aligned.
    if ((adr_to & 0xfffff000) != adr_to) {
        pr_crit("adr_to (%u) must be page-aligned.\n", adr_to);
        return -1; // Return -1 to indicate an error.
    }

    // Ensure that the zone_index is within the valid range.
    if ((zone_index < 0) || (zone_index >= contig_page_data->nr_zones)) {
        pr_crit("The zone_index (%d) is out of bounds (max: %d).\n",
                zone_index, contig_page_data->nr_zones - 1);
        return -1; // Return -1 to indicate an error.
    }

    // Take the zone_t structure that corresponds to the zone_index.
    zone_t *zone = contig_page_data->node_zones + zone_index;

    // Ensure that the zone was retrieved successfully.
    if (!zone) {
        pr_crit("Failed to retrieve the zone for zone_index: %d.\n", zone_index);
        return -1; // Return -1 to indicate an error.
    }

    // Calculate the number of page frames in the zone.
    size_t num_page_frames = (adr_to - adr_from) / PAGE_SIZE;

    // Calculate the index of the first page frame of the zone.
    uint32_t first_page_frame = adr_from / PAGE_SIZE;

    // Update zone information.
    zone->name           = name;                       // Set the zone's name.
    zone->size           = num_page_frames;            // Set the total number of page frames.
    zone->free_pages     = num_page_frames;            // Initialize free pages to the total number.
    zone->zone_mem_map   = mem_map + first_page_frame; // Map the memory for the zone.
    zone->zone_start_pfn = first_page_frame;           // Set the starting page frame number.

    // Clear the page structures in the memory map.
    memset(zone->zone_mem_map, 0, zone->size * sizeof(page_t));

    // Initialize the buddy system for the new zone.
    buddy_system_init(
        &zone->buddy_system,             // Buddy system structure for the zone.
        name,                            // Name of the zone.
        zone->zone_mem_map,              // Pointer to the memory map of the zone.
        BBSTRUCT_OFFSET(page_t, bbpage), // Offset for the buddy system structure.
        sizeof(page_t),                  // Size of each page.
        num_page_frames                  // Total number of page frames in the zone.
    );

    // Dump the current state of the buddy system for debugging purposes.
    buddy_system_dump(&zone->buddy_system);

    return 0;
}

/*
 * AAAABBBBCCCC
 *    ZZZZZZ
 *
 * */

unsigned int find_nearest_order_greater(uint32_t base_addr, uint32_t amount)
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
    unsigned int order = 0;
    while ((1UL << order) < npages) {
        ++order;
    }

    return order; // Return the calculated order.
}

int pmmngr_init(boot_info_t *boot_info)
{
    //=======================================================================
    lowmem_phy_start = boot_info->lowmem_phy_start;
    // Now we have skipped all modules in physical space, is time to consider
    // also virtual lowmem space!
    lowmem_virt_start = boot_info->lowmem_start;
    //=======================================================================

    //==== Initialize array of page_t =======================================
    pr_debug("Initializing low memory map structure...\n");
    mem_map = (page_t *)lowmem_virt_start;
    // Compute the size of memory.
    mem_size = boot_info->highmem_phy_end;
    // Total number of blocks (all lowmem+highmem RAM).
    mem_map_num = mem_size / PAGE_SIZE;
    // Initialize each page_t.
    for (unsigned i = 0; i < mem_map_num; ++i) {
        // Mark page as free.
        set_page_count(&mem_map[i], 0);
    }
    // Skip memory space used for page_t[]
    lowmem_phy_start += sizeof(page_t) * mem_map_num;
    lowmem_virt_start += sizeof(page_t) * mem_map_num;
    //=======================================================================

    //==== Initialize contig_page_data node =================================
    pr_debug("Initializing contig_page_data node...\n");
    contig_page_data = (pg_data_t *)lowmem_virt_start;
    // ZONE_NORMAL and ZONE_HIGHMEM
    contig_page_data->nr_zones = __MAX_NR_ZONES;
    // NID start from 0.
    contig_page_data->node_id = 0;
    // Corresponds with mem_map.
    contig_page_data->node_mem_map = mem_map;
    // In UMA we have only one node.
    contig_page_data->node_next = NULL;
    // All the memory.
    contig_page_data->node_size = mem_map_num;
    // mem_map[0].
    contig_page_data->node_start_mapnr = 0;
    // The first physical page.
    contig_page_data->node_start_paddr = 0x0;
    // Skip memory space used for pg_data_t.
    lowmem_phy_start += sizeof(pg_data_t);
    lowmem_virt_start += sizeof(pg_data_t);
    //=======================================================================

    pr_debug("Initializing zones...\n");

    //==== Initialize ZONE_NORMAL ==========================================
    // ZONE_NORMAL   [ memory_start - mem_size / 4 ]
    normal_start_addr = MAX_PAGE_ALIGN(lowmem_phy_start);
    normal_end_addr   = MIN_PAGE_ALIGN(boot_info->lowmem_phy_end);
    // Move the stop address so that the size is a multiple of max buddysystem
    // order.
    normal_size      = MIN_ORDER_ALIGN(normal_end_addr - normal_start_addr);
    normal_end_addr  = normal_start_addr + normal_size;
    lowmem_virt_base = lowmem_virt_start + (normal_start_addr - lowmem_phy_start);
    lowmem_page_base = normal_start_addr / PAGE_SIZE;
    //==== Initialize ZONE_HIGHMEM ==========================================
    // ZONE_HIGHMEM  [ mem_size/4 - mem_size ]
    high_start_addr = MAX_PAGE_ALIGN((uint32_t)boot_info->highmem_phy_start);
    high_end_addr   = MIN_PAGE_ALIGN(boot_info->highmem_phy_end);
    // Move the stop address so that the size is a multiple of max buddysystem
    // order.
    high_size     = MIN_ORDER_ALIGN(high_end_addr - high_start_addr);
    high_end_addr = high_start_addr + high_size;
    //=======================================================================

    zone_init("Normal", ZONE_NORMAL, normal_start_addr, normal_end_addr);
    zone_init("HighMem", ZONE_HIGHMEM, high_start_addr, high_end_addr);

    pr_debug("Memory addresses:\n");
    pr_debug("    LowMem  (phy): 0x%p to 0x%p\n", boot_info->lowmem_phy_start, boot_info->lowmem_phy_end);
    pr_debug("    HighMem (phy): 0x%p to 0x%p\n", boot_info->highmem_phy_start, boot_info->highmem_phy_end);
    pr_debug("    LowMem  (vrt): 0x%p to 0x%p\n", boot_info->lowmem_start, boot_info->lowmem_end);
    pr_debug("Memory map  size      : %s\n", to_human_size(sizeof(page_t) * mem_map_num));
    pr_debug("Memory size           : %s\n", to_human_size(mem_size));
    pr_debug("Page size             : %s\n", to_human_size(PAGE_SIZE));
    pr_debug("Number of page frames : %u\n", mem_map_num);
    for (unsigned i = 0; i < __MAX_NR_ZONES; ++i) {
        zone_t *zone = &contig_page_data->node_zones[i];
        pr_debug("Zone %9s, first page: 0x%p, last page: 0x%p, # pages: %6d\n",
                 zone->name,
                 zone->zone_mem_map,
                 zone->zone_mem_map + zone->size,
                 zone->size);
    }
    return 1;
}

page_t *alloc_page_cached(gfp_t gfp_mask)
{
    // Get the zone corresponding to the given GFP mask.
    zone_t *zone = get_zone_from_flags(gfp_mask);

    // Ensure the zone is valid.
    if (!zone) {
        pr_crit("Failed to get zone from GFP mask.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Allocate a page from the buddy system of the zone.
    bb_page_t *bbpage = bb_alloc_page_cached(&zone->buddy_system);

    // Ensure the allocation was successful.
    if (!bbpage) {
        pr_crit("Failed to allocate page from buddy system.\n");
        return NULL; // Return NULL to indicate failure.
    }

    // Convert the buddy system page structure to the page_t structure.
    return PG_FROM_BBSTRUCT(bbpage, page_t, bbpage);
}

int free_page_cached(page_t *page)
{
    // Ensure the page pointer is not NULL.
    if (!page) {
        pr_crit("Invalid page pointer: NULL.\n");
        return -1; // Return -1 to indicate failure.
    }

    // Get the zone that contains the given page.
    zone_t *zone = get_zone_from_page(page);

    // Ensure the zone is valid.
    if (!zone) {
        pr_crit("Failed to get zone from page.\n");
        return -1; // Return -1 to indicate failure.
    }

    // Free the page from the buddy system of the zone.
    bb_free_page_cached(&zone->buddy_system, &page->bbpage);

    return 0; // Return success.
}

uint32_t __alloc_page_lowmem(gfp_t gfp_mask)
{
    // Allocate a cached page based on the given GFP mask.
    page_t *page = alloc_page_cached(gfp_mask);

    // Ensure the page allocation was successful.
    if (!page) {
        pr_crit("Failed to allocate low memory page.\n");
        return 0; // Return 0 to indicate failure.
    }

    // Get the low memory address from the allocated page.
    return get_virtual_address_from_page(page);
}

int free_page_lowmem(uint32_t addr)
{
    // Get the page corresponding to the given low memory address.
    page_t *page = get_page_from_virtual_address(addr);

    // Ensure the page retrieval was successful.
    if (!page) {
        pr_crit("Failed to retrieve page from address: 0x%x\n", addr);
        return -1; // Return -1 to indicate failure.
    }

    // Free the cached page.
    free_page_cached(page);

    return 0; // Return success.
}

uint32_t __alloc_pages_lowmem(gfp_t gfp_mask, uint32_t order)
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
    page_t *page = _alloc_pages(gfp_mask, order);

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

page_t *_alloc_pages(gfp_t gfp_mask, uint32_t order)
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

#if 0
    pr_warning("BS-A: (page: %p order: %d)\n", page, order);
#endif

    // Return the pointer to the first page in the allocated block.
    return page;
}

int free_pages_lowmem(uint32_t addr)
{
    // Get the page corresponding to the given low memory address.
    page_t *page = get_page_from_virtual_address(addr);

    // Ensure the page retrieval was successful.
    if (!page) {
        pr_emerg("Failed to retrieve page from address: 0x%x. Page is over memory size.\n", addr);
        return -1; // Return -1 to indicate failure.
    }

    // Free the pages starting from the given page.
    __free_pages(page);

    return 0; // Return success.
}

int __free_pages(page_t *page)
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

#if 0
    pr_warning("BS-F: (page: %p order: %d)\n", page, order);
#endif

    return 0; // Return success.
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
