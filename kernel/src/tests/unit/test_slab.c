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

/// @brief Main test function for slab subsystem.
void test_slab(void)
{
    test_memory_slab_cache_alloc_free();
    test_memory_kmalloc_kfree();
    test_memory_kmalloc_write_read();
    test_memory_slab_ctor_dtor();
    test_memory_slab_counters();
    test_memory_slab_stress();
}
