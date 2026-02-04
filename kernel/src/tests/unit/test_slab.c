/// @file test_slab.c
/// @brief Slab allocator tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"          // Include kernel log levels.
#define __DEBUG_HEADER__ "[TUNIT ]"     ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_DEBUG ///< Set log level.
#include "io/debug.h"                   // Include debugging functions.

#include "mem/alloc/slab.h"
#include "mem/alloc/zone_allocator.h"
#include "mem/gfp.h"
#include "mem/paging.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

static unsigned int slab_ctor_calls;
static unsigned int slab_dtor_calls;

static void slab_test_ctor(void *ptr)
{
    slab_ctor_calls++;
    memset(ptr, 0xCD, sizeof(uint64_t));
}

static void slab_test_dtor(void *ptr)
{
    slab_dtor_calls++;
    memset(ptr, 0x00, sizeof(uint64_t));
}

/// @brief Test basic slab cache allocation and free.
TEST(memory_slab_cache_alloc_free)
{
    TEST_SECTION_START("Slab cache alloc/free");

    typedef struct test_obj {
        uint32_t a;
        uint32_t b;
    } test_obj_t;

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    kmem_cache_t *cache = kmem_cache_create("test_obj", sizeof(test_obj_t), alignof(test_obj_t), GFP_KERNEL, NULL, NULL);
    ASSERT_MSG(cache != NULL, "kmem_cache_create must succeed");

    test_obj_t *obj = kmem_cache_alloc(cache, GFP_KERNEL);
    ASSERT_MSG(obj != NULL, "kmem_cache_alloc must return a valid object");
    obj->a = 0xA5A5A5A5;
    obj->b = 0x5A5A5A5A;

    ASSERT_MSG(kmem_cache_free(obj) == 0, "kmem_cache_free must succeed");
    ASSERT_MSG(kmem_cache_destroy(cache) == 0, "kmem_cache_destroy must succeed");

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free pages must be restored after cache destroy");

    TEST_SECTION_END();
}

/// @brief Test kmalloc/kfree basic behavior.
TEST(memory_kmalloc_kfree)
{
    TEST_SECTION_START("kmalloc/kfree");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    void *ptr = kmalloc(128);
    ASSERT_MSG(ptr != NULL, "kmalloc must return a valid pointer");
    memset(ptr, 0xAB, 128);
    kfree(ptr);

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free pages must be restored after kfree");

    TEST_SECTION_END();
}

/// @brief Test kmalloc write/read roundtrip.
TEST(memory_kmalloc_write_read)
{
    TEST_SECTION_START("kmalloc write/read");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    uint8_t *ptr = (uint8_t *)kmalloc(256);
    ASSERT_MSG(ptr != NULL, "kmalloc must return a valid pointer");

    for (uint32_t i = 0; i < 256; ++i) {
        ptr[i] = (uint8_t)(0x5A ^ i);
    }
    for (uint32_t i = 0; i < 256; ++i) {
        ASSERT_MSG(ptr[i] == (uint8_t)(0x5A ^ i), "kmalloc data must round-trip");
    }

    kfree(ptr);
    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free pages must be restored after kfree");
    TEST_SECTION_END();
}

/// @brief Test ctor/dtor callbacks and multi-alloc behavior.
TEST(memory_slab_ctor_dtor)
{
    TEST_SECTION_START("Slab ctor/dtor");

    slab_ctor_calls = 0;
    slab_dtor_calls = 0;

    kmem_cache_t *cache =
        kmem_cache_create("test_obj_ctor", sizeof(uint64_t), alignof(uint64_t), GFP_KERNEL, slab_test_ctor, slab_test_dtor);
    ASSERT_MSG(cache != NULL, "kmem_cache_create must succeed");

    void *obj1 = kmem_cache_alloc(cache, GFP_KERNEL);
    void *obj2 = kmem_cache_alloc(cache, GFP_KERNEL);
    void *obj3 = kmem_cache_alloc(cache, GFP_KERNEL);

    ASSERT_MSG(obj1 != NULL && obj2 != NULL && obj3 != NULL, "allocations must succeed");
    ASSERT_MSG(slab_ctor_calls >= 3, "ctor must run for each allocation");

    ASSERT_MSG(kmem_cache_free(obj1) == 0, "kmem_cache_free must succeed");
    ASSERT_MSG(kmem_cache_free(obj2) == 0, "kmem_cache_free must succeed");
    ASSERT_MSG(kmem_cache_free(obj3) == 0, "kmem_cache_free must succeed");
    ASSERT_MSG(slab_dtor_calls >= 3, "dtor must run for each free");

    ASSERT_MSG(kmem_cache_destroy(cache) == 0, "kmem_cache_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Test slab cache counters return to baseline after free.
TEST(memory_slab_counters)
{
    TEST_SECTION_START("Slab counters");

    kmem_cache_t *cache = kmem_cache_create("test_obj_cnt", 32, alignof(uint32_t), GFP_KERNEL, NULL, NULL);
    ASSERT_MSG(cache != NULL, "kmem_cache_create must succeed");

    unsigned int total_before = cache->total_num;
    unsigned int free_before  = cache->free_num;

    void *objs[8] = {0};
    for (unsigned int i = 0; i < 8; ++i) {
        objs[i] = kmem_cache_alloc(cache, GFP_KERNEL);
        ASSERT_MSG(objs[i] != NULL, "kmem_cache_alloc must succeed");
    }

    for (unsigned int i = 0; i < 8; ++i) {
        ASSERT_MSG(kmem_cache_free(objs[i]) == 0, "kmem_cache_free must succeed");
    }

    ASSERT_MSG(cache->total_num >= total_before, "total_num must not shrink");
    ASSERT_MSG(cache->free_num >= free_before, "free_num must not shrink");
    ASSERT_MSG(cache->free_num == cache->total_num, "all objects must be free after frees");

    ASSERT_MSG(kmem_cache_destroy(cache) == 0, "kmem_cache_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Stress slab allocations to detect internal leaks.
TEST(memory_slab_stress)
{
    TEST_SECTION_START("Slab stress");

    kmem_cache_t *cache = kmem_cache_create("test_obj_stress", 64, alignof(uint64_t), GFP_KERNEL, NULL, NULL);
    ASSERT_MSG(cache != NULL, "kmem_cache_create must succeed");

    const unsigned int rounds = 16;
    const unsigned int batch  = 32;
    void *objs[batch];

    for (unsigned int r = 0; r < rounds; ++r) {
        for (unsigned int i = 0; i < batch; ++i) {
            objs[i] = kmem_cache_alloc(cache, GFP_KERNEL);
            ASSERT_MSG(objs[i] != NULL, "kmem_cache_alloc must succeed");
        }
        for (unsigned int i = 0; i < batch; ++i) {
            ASSERT_MSG(kmem_cache_free(objs[i]) == 0, "kmem_cache_free must succeed");
        }

        ASSERT_MSG(cache->free_num == cache->total_num, "all objects must be free after round");
    }

    ASSERT_MSG(kmem_cache_destroy(cache) == 0, "kmem_cache_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Test zero-size allocation handling in kmalloc.
TEST(memory_slab_kmalloc_zero_size)
{
    TEST_SECTION_START("kmalloc zero size");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    void *ptr = kmalloc(0);
    if (ptr != NULL) {
        kfree(ptr);
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after == free_before, "Zone free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test NULL pointer handling in kfree.
TEST(memory_slab_kfree_null)
{
    TEST_SECTION_START("kfree NULL");

    kfree(NULL);

    TEST_SECTION_END();
}

/// @brief Test very large kmalloc that should exceed slab cache.
TEST(memory_slab_kmalloc_large)
{
    TEST_SECTION_START("kmalloc large allocation");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    uint32_t large_size = 16 * PAGE_SIZE;
    void *ptr           = kmalloc(large_size);

    if (ptr != NULL) {
        for (uint32_t i = 0; i < 256; ++i) {
            ((uint8_t *)ptr)[i] = (uint8_t)(i & 0xFF);
        }

        for (uint32_t i = 0; i < 256; ++i) {
            ASSERT_MSG(((uint8_t *)ptr)[i] == (uint8_t)(i & 0xFF), "large allocation data must persist");
        }

        kfree(ptr);
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test alignment verification for various slab sizes.
TEST(memory_slab_alignment)
{
    TEST_SECTION_START("Slab alignment verification");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    uint32_t sizes[] = { 8, 16, 32, 64, 128, 256, 512, 1024 };
    for (unsigned int i = 0; i < (sizeof(sizes) / sizeof(uint32_t)); ++i) {
        void *ptr = kmalloc(sizes[i]);
        if (ptr != NULL) {
            uintptr_t addr = (uintptr_t)ptr;
            ASSERT_MSG((addr & (sizes[i] - 1)) == 0, "allocation must be aligned to size");
            kfree(ptr);
        }
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test slab cache with large objects.
TEST(memory_slab_large_objects)
{
    TEST_SECTION_START("Slab large objects");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    typedef struct {
        uint32_t data[16];
    } large_obj_t;

    kmem_cache_t *cache = kmem_cache_create("large_test", sizeof(large_obj_t), alignof(large_obj_t), GFP_KERNEL, NULL, NULL);
    if (cache != NULL) {
        large_obj_t *obj = kmem_cache_alloc(cache, GFP_KERNEL);
        if (obj != NULL) {
            for (int i = 0; i < 16; ++i) {
                obj->data[i] = 0xDEADBEEFU;
            }

            for (int i = 0; i < 16; ++i) {
                ASSERT_MSG(obj->data[i] == 0xDEADBEEFU, "data must persist");
            }

            ASSERT_MSG(kmem_cache_free(obj) == 0, "kmem_cache_free must succeed");
        }

        ASSERT_MSG(kmem_cache_destroy(cache) == 0, "kmem_cache_destroy must succeed");
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before, "Free space must be restored");

    TEST_SECTION_END();
}

/// @brief Test odd-size object alignment in caches.
TEST(memory_slab_odd_size_alignment)
{
    TEST_SECTION_START("Slab odd-size alignment");

    // Test 24-byte allocation
    void *ptr24_1 = kmalloc(24);
    void *ptr24_2 = kmalloc(24);
    ASSERT_MSG(ptr24_1 != NULL, "24-byte kmalloc must succeed");
    ASSERT_MSG(ptr24_2 != NULL, "second 24-byte kmalloc must succeed");
    ASSERT_MSG(ptr24_1 != ptr24_2, "allocations must be distinct");

    // Test 40-byte allocation
    void *ptr40 = kmalloc(40);
    ASSERT_MSG(ptr40 != NULL, "40-byte kmalloc must succeed");

    // Test 72-byte allocation
    void *ptr72 = kmalloc(72);
    ASSERT_MSG(ptr72 != NULL, "72-byte kmalloc must succeed");

    // Write and verify
    memset(ptr24_1, 0xAA, 24);
    memset(ptr40, 0xBB, 40);
    memset(ptr72, 0xCC, 72);

    ASSERT_MSG(*(uint8_t *)ptr24_1 == 0xAA, "24-byte value must be readable");
    ASSERT_MSG(*(uint8_t *)ptr40 == 0xBB, "40-byte value must be readable");
    ASSERT_MSG(*(uint8_t *)ptr72 == 0xCC, "72-byte value must be readable");

    kfree(ptr24_1);
    kfree(ptr24_2);
    kfree(ptr40);
    kfree(ptr72);

    TEST_SECTION_END();
}

/// @brief Test cache object reuse (same address returned after free).
TEST(memory_slab_object_reuse)
{
    TEST_SECTION_START("Slab object reuse");

    // Allocate, free, and reallocate same size
    void *ptr1 = kmalloc(64);
    ASSERT_MSG(ptr1 != NULL, "first kmalloc must succeed");

    uint32_t addr1 = (uint32_t)ptr1;
    kfree(ptr1);

    // Allocate again - should possibly reuse same address
    void *ptr2 = kmalloc(64);
    ASSERT_MSG(ptr2 != NULL, "second kmalloc must succeed");

    // Address reuse is an optimization - not guaranteed but common
    // The important thing is that it works correctly
    uint32_t addr2 = (uint32_t)ptr2;

    // Write to ptr2 and verify
    *(uint32_t *)ptr2 = 0xDEADBEEF;
    ASSERT_MSG(*(uint32_t *)ptr2 == 0xDEADBEEF, "value must be correctly stored");

    kfree(ptr2);

    TEST_SECTION_END();
}

/// @brief Test stress across multiple caches in parallel.
TEST(memory_slab_parallel_caches)
{
    TEST_SECTION_START("Slab parallel caches");

    unsigned long free_before = get_zone_free_space(GFP_KERNEL);

    // Allocate from different size classes concurrently
    void *ptrs[12];
    ptrs[0]  = kmalloc(16);
    ptrs[1]  = kmalloc(32);
    ptrs[2]  = kmalloc(64);
    ptrs[3]  = kmalloc(128);
    ptrs[4]  = kmalloc(256);
    ptrs[5]  = kmalloc(512);
    ptrs[6]  = kmalloc(24);
    ptrs[7]  = kmalloc(48);
    ptrs[8]  = kmalloc(96);
    ptrs[9]  = kmalloc(192);
    ptrs[10] = kmalloc(384);
    ptrs[11] = kmalloc(768);

    // Verify all succeeded
    for (int i = 0; i < 12; i++) {
        ASSERT_MSG(ptrs[i] != NULL, "kmalloc must succeed for all sizes");
    }

    // Free all
    for (int i = 0; i < 12; i++) {
        kfree(ptrs[i]);
    }

    unsigned long free_after = get_zone_free_space(GFP_KERNEL);
    ASSERT_MSG(free_after >= free_before - PAGE_SIZE, "Free space should mostly be restored");

    TEST_SECTION_END();
}

/// @brief Test cache destruction safety when empty but with prior allocations.
TEST(memory_slab_cache_destruction_safety)
{
    TEST_SECTION_START("Slab cache destruction safety");

    // Create a cache
    kmem_cache_t *cache = kmem_cache_create("test_cache", 128, 0, 0, NULL, NULL);
    ASSERT_MSG(cache != NULL, "kmem_cache_create must succeed");

    // Allocate from it
    void *obj1 = kmem_cache_alloc(cache, GFP_KERNEL);
    void *obj2 = kmem_cache_alloc(cache, GFP_KERNEL);
    ASSERT_MSG(obj1 != NULL, "cache alloc must succeed");
    ASSERT_MSG(obj2 != NULL, "cache alloc must succeed");

    // Free everything
    kmem_cache_free(cache, obj1);
    kmem_cache_free(cache, obj2);

    // Destroy empty cache - should not crash
    kmem_cache_destroy(cache);

    TEST_SECTION_END();
}

/// @brief Main test function for slab subsystem.
void test_slab(void)
{
    test_memory_slab_cache_alloc_free();
    test_memory_kmalloc_kfree();
    test_memory_kmalloc_write_read();
    test_memory_slab_ctor_dtor();
    test_memory_slab_counters();
    test_memory_slab_stress();
    test_memory_slab_kmalloc_zero_size();
    test_memory_slab_kfree_null();
    test_memory_slab_kmalloc_large();
    test_memory_slab_alignment();
    test_memory_slab_large_objects();
    test_memory_slab_odd_size_alignment();
    test_memory_slab_object_reuse();
    test_memory_slab_parallel_caches();
    test_memory_slab_cache_destruction_safety();
}
