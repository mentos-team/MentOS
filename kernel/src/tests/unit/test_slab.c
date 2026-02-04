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
#include "mem/gfp.h"
#include "string.h"
#include "tests/test.h"
#include "tests/test_utils.h"

/// @brief Test basic slab cache allocation and free.
TEST(memory_slab_cache_alloc_free)
{
    TEST_SECTION_START("Slab cache alloc/free");

    typedef struct test_obj {
        uint32_t a;
        uint32_t b;
    } test_obj_t;

    kmem_cache_t *cache = kmem_cache_create("test_obj", sizeof(test_obj_t), alignof(test_obj_t), GFP_KERNEL, NULL, NULL);
    ASSERT_MSG(cache != NULL, "kmem_cache_create must succeed");

    test_obj_t *obj = kmem_cache_alloc(cache, GFP_KERNEL);
    ASSERT_MSG(obj != NULL, "kmem_cache_alloc must return a valid object");
    obj->a = 0xA5A5A5A5;
    obj->b = 0x5A5A5A5A;

    ASSERT_MSG(kmem_cache_free(obj) == 0, "kmem_cache_free must succeed");
    ASSERT_MSG(kmem_cache_destroy(cache) == 0, "kmem_cache_destroy must succeed");

    TEST_SECTION_END();
}

/// @brief Test kmalloc/kfree basic behavior.
TEST(memory_kmalloc_kfree)
{
    TEST_SECTION_START("kmalloc/kfree");

    void *ptr = kmalloc(128);
    ASSERT_MSG(ptr != NULL, "kmalloc must return a valid pointer");
    memset(ptr, 0xAB, 128);
    kfree(ptr);

    TEST_SECTION_END();
}

/// @brief Main test function for slab subsystem.
void test_slab(void)
{
    test_memory_slab_cache_alloc_free();
    test_memory_kmalloc_kfree();
}
