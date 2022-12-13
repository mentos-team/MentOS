/// @file zone_allocator.c
/// @brief Implementation of the Zone Allocator
/// @copyright (c) 2014-2022 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Include the kernel log levels.
#include "sys/kernel_levels.h"
/// Change the header.
#define __DEBUG_HEADER__ "[PMM   ]"
/// Set the log level.
#define __DEBUG_LEVEL__ LOGLEVEL_NOTICE

#include "mem/zone_allocator.h"
#include "mem/buddysystem.h"
#include "klib/list_head.h"
#include "kernel.h"
#include "assert.h"
#include "mem/paging.h"
#include "string.h"
#include "io/debug.h"

/// TODO: Comment.
#define MIN_PAGE_ALIGN(addr) ((addr) & (~(PAGE_SIZE - 1)))
/// TODO: Comment.
#define MAX_PAGE_ALIGN(addr) (((addr) & (~(PAGE_SIZE - 1))) + PAGE_SIZE)
/// TODO: Comment.
#define MIN_ORDER_ALIGN(addr) ((addr) & (~((PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)) - 1)))
/// TODO: Comment.
#define MAX_ORDER_ALIGN(addr)                                             \
    (((addr) & (~((PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)) - 1))) + \
     (PAGE_SIZE << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)))

/// Array of all physical blocks
page_t *mem_map = NULL;
/// Memory node.
pg_data_t *contig_page_data = NULL;
/// Low memory virtual base address.
uint32_t lowmem_virt_base = 0;
/// Low memory base address.
uint32_t lowmem_page_base = 0;

page_t *get_lowmem_page_from_address(uint32_t addr)
{
    unsigned int offset = addr - lowmem_virt_base;
    return mem_map + lowmem_page_base + (offset / PAGE_SIZE);
}

uint32_t get_lowmem_address_from_page(page_t *page)
{
    unsigned int offset = (page - mem_map) - lowmem_page_base;
    return lowmem_virt_base + offset * PAGE_SIZE;
}

uint32_t get_physical_address_from_page(page_t *page)
{
    return (page - mem_map) * PAGE_SIZE;
}

page_t *get_page_from_physical_address(uint32_t phy_addr)
{
    return mem_map + (phy_addr / PAGE_SIZE);
}

/// @brief Get the zone that contains a page frame.
/// @param page A page descriptor.
/// @return The zone requested.
static zone_t *get_zone_from_page(page_t *page)
{
    zone_t *zone;
    page_t *last_page;
    // Iterate over all the zones.
    for (int zone_index = 0; zone_index < contig_page_data->nr_zones; zone_index++) {
        // Get the zone at the given index.
        zone = contig_page_data->node_zones + zone_index;
        assert(zone && "Failed to retrieve the zone.");
        // Get the last page of the zone.
        last_page = zone->zone_mem_map + zone->size;
        assert(last_page && "Failed to retrieve the last page of the zone.");
        // Check if the page is before the last page of the zone.
        if (page < last_page)
            return zone;
    }
    // Error: page is over memory size.
    return (zone_t *)NULL;
}

/// @brief Get a zone from gfp_mask
/// @param gfp_mask GFP_FLAG see gfp.h.
/// @return The zone requested.
static zone_t *get_zone_from_flags(gfp_t gfp_mask)
{
    switch (gfp_mask) {
    case GFP_KERNEL:
    case GFP_ATOMIC:
    case GFP_NOFS:
    case GFP_NOIO:
    case GFP_NOWAIT:
        return &contig_page_data->node_zones[ZONE_NORMAL];
    case GFP_HIGHUSER:
        return &contig_page_data->node_zones[ZONE_HIGHMEM];
    default:
        return (zone_t *)NULL;
    }
}

static int is_memory_clean(gfp_t gfp_mask)
{
    // Get the corresponding zone.
    zone_t *zone = get_zone_from_flags(gfp_mask);
    assert(zone && "Failed to retrieve the zone given the gfp_mask!");
    // Get the last free area list of the buddy system.
    bb_free_area_t *area = zone->buddy_system.free_area + (MAX_BUDDYSYSTEM_GFP_ORDER - 1);
    assert(area && "Failed to retrieve the last free_area for the given zone!");
    // Compute the total size of the zone.
    unsigned int total_size = (zone->size / (1UL << (MAX_BUDDYSYSTEM_GFP_ORDER - 1)));
    // Check if the size of the zone is equal to the remaining pages inside the free area.
    if (area->nr_free != total_size) {
        pr_crit("Number of blocks of free pages is different than expected (%d vs %d).\n", area->nr_free, total_size);
        buddy_system_dump(&zone->buddy_system);
        return 0;
    }
    return 1;
}

/// @brief  Checks if the physical memory manager is working properly.
/// @return If the check was done correctly.
static int pmm_check()
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
    free_page_lowmem((uint32_t)ptr1);

    if (!is_memory_clean(GFP_KERNEL)) {
        pr_emerg("Test failed, memory is not clean.\n");
        return 0;
    }
    return 1;
}

/// @brief Initializes the memory attributes.
/// @param name       Zone's name.
/// @param zone_index Zone's index.
/// @param adr_from   the lowest address of the zone
/// @param adr_to     the highest address of the zone (not included!)
static void zone_init(char *name, int zone_index, uint32_t adr_from, uint32_t adr_to)
{
    assert((adr_from < adr_to) && "Inserted bad block addresses!");
    assert(((adr_from & 0xfffff000) == adr_from) && "Inserted bad block addresses!");
    assert(((adr_to & 0xfffff000) == adr_to) && "Inserted bad block addresses!");
    assert((zone_index < contig_page_data->nr_zones) && "The index is above the number of zones.");
    // Take the zone_t structure that correspondes to the zone_index.
    zone_t *zone = contig_page_data->node_zones + zone_index;
    assert(zone && "Failed to retrieve the zone.");
    // Number of page frames in the zone.
    size_t num_page_frames = (adr_to - adr_from) / PAGE_SIZE;
    // Index of the first page frame of the zone.
    uint32_t first_page_frame = adr_from / PAGE_SIZE;
    // Update zone info.
    zone->name           = name;
    zone->size           = num_page_frames;
    zone->free_pages     = num_page_frames;
    zone->zone_mem_map   = mem_map + first_page_frame;
    zone->zone_start_pfn = first_page_frame;
    // Dump the information.
    pr_debug("ZONE %s, first page: %p, last page: %p, npages:%d\n", zone->name,
             zone->zone_mem_map, zone->zone_mem_map + zone->size, zone->size);
    // Set to zero all page structures.
    memset(zone->zone_mem_map, 0, zone->size * sizeof(page_t));
    // Initialize the buddy system for the new zone.
    buddy_system_init(&zone->buddy_system,
                      name,
                      zone->zone_mem_map,
                      BBSTRUCT_OFFSET(page_t, bbpage),
                      sizeof(page_t),
                      num_page_frames);
    buddy_system_dump(&zone->buddy_system);
}

/*
 * AAAABBBBCCCC
 *    ZZZZZZ
 *
 * */

unsigned int find_nearest_order_greater(uint32_t base_addr, uint32_t amount)
{
    uint32_t start_pfn = base_addr / PAGE_SIZE;
    uint32_t end_pfn   = (base_addr + amount + PAGE_SIZE - 1) / PAGE_SIZE;
    // Get the number of pages.
    uint32_t npages = end_pfn - start_pfn;
    // Find the fitting order.
    unsigned int order = 0;
    while ((1UL << order) < npages) {
        ++order;
    }
    return order;
}

int pmmngr_init(boot_info_t *boot_info)
{
    //=======================================================================

    uint32_t lowmem_phy_start = boot_info->lowmem_phy_start;

    // Now we have skipped all modules in physical space, is time to
    // consider also virtual lowmem space!
    uint32_t lowmem_virt_start = boot_info->lowmem_start + (lowmem_phy_start - boot_info->lowmem_phy_start);

    pr_debug("Start memory address after skip modules (phy => virt) : 0x%p => 0x%p \n",
             lowmem_phy_start, lowmem_virt_start);
    //=======================================================================

    //==== Initialize array of page_t =======================================
    pr_debug("Initializing low memory map structure...\n");
    mem_map = (page_t *)lowmem_virt_start;

    uint32_t mem_size = boot_info->highmem_phy_end;

    // Total number of blocks (all lowmem+highmem RAM).
    uint32_t mem_num_frames = mem_size / PAGE_SIZE;

    // Initialize each page_t.
    for (int page_index = 0; page_index < mem_num_frames; ++page_index) {
        page_t *page = mem_map + page_index;
        // Mark page as free.
        set_page_count(page, 0);
    }
    //=======================================================================

    //==== Skip memory space used for page_t[] ==============================
    lowmem_phy_start += sizeof(page_t) * mem_num_frames;
    lowmem_virt_start += sizeof(page_t) * mem_num_frames;
    pr_debug("Size of mem_map                            : %i byte [0x%p - 0x%p]\n",
             (char *)lowmem_virt_start - (char *)mem_map, mem_map,
             lowmem_virt_start);
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
    contig_page_data->node_size = mem_num_frames;
    // mem_map[0].
    contig_page_data->node_start_mapnr = 0;
    // The first physical page.
    contig_page_data->node_start_paddr = 0x0;
    //=======================================================================

    //==== Skip memory space used for pg_data_t =============================
    lowmem_phy_start += sizeof(pg_data_t);
    lowmem_virt_start += sizeof(pg_data_t);
    //=======================================================================

    //==== Initialize zones zone_t ==========================================
    pr_debug("Initializing zones...\n");

    // ZONE_NORMAL   [ memory_start - mem_size/4 ]
    uint32_t start_normal_addr = MAX_PAGE_ALIGN(lowmem_phy_start);
    uint32_t stop_normal_addr  = MIN_PAGE_ALIGN(boot_info->lowmem_phy_end);

    // Move the stop address so that the size is a multiple of max buddysystem order
    uint32_t normal_area_size = MIN_ORDER_ALIGN(stop_normal_addr - start_normal_addr);
    stop_normal_addr          = start_normal_addr + normal_area_size;

    uint32_t phv_delta = start_normal_addr - lowmem_phy_start;
    lowmem_virt_base   = lowmem_virt_start + phv_delta;
    lowmem_page_base   = start_normal_addr / PAGE_SIZE;
    zone_init("Normal", ZONE_NORMAL, start_normal_addr, stop_normal_addr);

    // ZONE_HIGHMEM  [ mem_size/4 - mem_size ]
    uint32_t start_high_addr = MAX_PAGE_ALIGN((uint32_t)boot_info->highmem_phy_start);
    uint32_t stop_high_addr  = MIN_PAGE_ALIGN(boot_info->highmem_phy_end);

    // Move the stop address so that the size is a multiple of max buddysystem order
    uint32_t high_area_size = MIN_ORDER_ALIGN(stop_high_addr - start_high_addr);
    stop_high_addr          = start_high_addr + high_area_size;

    zone_init("HighMem", ZONE_HIGHMEM, start_high_addr, stop_high_addr);
    //=======================================================================

    pr_debug("Memory Size                                : %u MB \n", mem_size / M);
    pr_debug("Total page frames    (MemorySize/4096)     : %u \n", mem_num_frames);
    pr_debug("mem_map address                            : 0x%p \n", mem_map);
    pr_debug("Memory Start                               : 0x%p \n", lowmem_phy_start);

    // With the caching enabled, the pmm check is useless.
    //return pmm_check();
    return 1;
}

page_t *alloc_page_cached(gfp_t gfp_mask)
{
    zone_t *zone = get_zone_from_flags(gfp_mask);
    return PG_FROM_BBSTRUCT(bb_alloc_page_cached(&zone->buddy_system), page_t, bbpage);
}

void free_page_cached(page_t *page)
{
    zone_t *zone = get_zone_from_page(page);
    bb_free_page_cached(&zone->buddy_system, &page->bbpage);
}

uint32_t __alloc_page_lowmem(gfp_t gfp_mask)
{
    return get_lowmem_address_from_page(alloc_page_cached(gfp_mask));
}

void free_page_lowmem(uint32_t addr)
{
    page_t *page = get_lowmem_page_from_address(addr);
    free_page_cached(page);
}

uint32_t __alloc_pages_lowmem(gfp_t gfp_mask, uint32_t order)
{
    assert((order <= (MAX_BUDDYSYSTEM_GFP_ORDER - 1)) && gfp_mask == GFP_KERNEL && "Order is exceeding limit.");

    page_t *page = _alloc_pages(gfp_mask, order);

    // Get the index of the first page frame of the block.
    uint32_t block_frame_adr = get_lowmem_address_from_page(page);
    if (block_frame_adr == -1) {
        pr_emerg("MEM. REQUEST FAILED");
    }
#if 0
    else {
        pr_debug("BS-G: addr: %p (page: %p order: %d)\n", block_frame_adr, page, order);
    }
#endif
    return block_frame_adr;
}

page_t *_alloc_pages(gfp_t gfp_mask, uint32_t order)
{
    uint32_t block_size = 1UL << order;

    zone_t *zone = get_zone_from_flags(gfp_mask);
    page_t *page = NULL;

    // Search for a block of page frames by using the BuddySystem.
    page = PG_FROM_BBSTRUCT(bb_alloc_pages(&zone->buddy_system, order), page_t, bbpage);

    // Set page counters
    for (int i = 0; i < block_size; i++) {
        set_page_count(&page[i], 1);
    }

    assert(page && "Cannot allocate pages.");

    // Decrement the number of pages in the zone.
    if (page) {
        zone->free_pages -= block_size;
    }

    return page;
}

void free_pages_lowmem(uint32_t addr)
{
    page_t *page = get_lowmem_page_from_address(addr);
    assert(page && "Page is over memory size.");
    __free_pages(page);
}

void __free_pages(page_t *page)
{
    zone_t *zone = get_zone_from_page(page);
    assert(zone && "Page is over memory size.");

    assert(zone->zone_mem_map <= page && "Page is below the selected zone!");

    uint32_t order      = page->bbpage.order;
    uint32_t block_size = 1UL << order;

    for (int i = 0; i < block_size; i++) {
        set_page_count(&page[i], 0);
    }

    bb_free_pages(&zone->buddy_system, &page->bbpage);

    zone->free_pages += block_size;
#if 0
    pr_debug("BS-F: (page: %p order: %d)\n", page, order);
#endif
    //buddy_system_dump(&zone->buddy_system);
}

unsigned long get_zone_total_space(gfp_t gfp_mask)
{
    zone_t *zone = get_zone_from_flags(gfp_mask);
    assert(zone && "Cannot retrieve the correct zone.");
    return buddy_system_get_total_space(&zone->buddy_system);
}

unsigned long get_zone_free_space(gfp_t gfp_mask)
{
    zone_t *zone = get_zone_from_flags(gfp_mask);
    assert(zone && "Cannot retrieve the correct zone.");
    return buddy_system_get_free_space(&zone->buddy_system);
}

unsigned long get_zone_cached_space(gfp_t gfp_mask)
{
    zone_t *zone = get_zone_from_flags(gfp_mask);
    assert(zone && "Cannot retrieve the correct zone.");
    return buddy_system_get_cached_space(&zone->buddy_system);
}
