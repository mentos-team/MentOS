#include <experimental/deadlock_prevention.h>
/**
 * @author Mirco De Marchi
 * @date   2/02/2021
 * @brief  Source file for deadlock prevention algorithms.
 * @copyright (c) University of Verona
 *
 * Complete request() and state_safe() functions, according to what you learnt
 * in deadlock lectures.
 *
 * In order to complete the functions I prepared for you a library to manage
 * easily arrays. The library is arr_math.h and you can find the documentation
 * in include/util/arr_math.h header file.
 *
 * In addition you will find the same data structures that you have seen in
 * class:
 * - available: number of instances of each resources currently available;
 * - max: matrix of the maximumun resources instances that each task may
 * require;
 * - alloc: matrix of current resource allocation for each task.
 * - need: matrix of current resource needs for each task.
 * Assume that these data structure are already filled with the
 * informations described, in other words, you need to use these data structures
 * in read-only.
 *
 * Suggestion!
 * From arr_math.h you may need only: arr_g_any(), arr_add(), arr_sub(),
 * all() and arr_ne();
 */

#include "deadlock_prevention.h"

#include "arr_math.h"
#include "kheap.h"
//------------------------------------------------------------------------------
// The following structure will be initialized during the system initialization.
uint32_t *  available = NULL;
uint32_t ** max       = NULL;
uint32_t ** alloc     = NULL;
uint32_t ** need      = NULL;

//------------------------------------------------------------------------------
/**
 * @brief Check if the current system resource allocation mantains the system in
 * a safe state.
 * @param n Number of tasks currently in the system (length of req_vec).
 * @param m Number of resource types in the system.
 */
static bool_t state_safe(size_t n, size_t m);
//------------------------------------------------------------------------------


deadlock_status_t request(uint32_t *req_vec, size_t task_i, size_t n, size_t m)
{
    if (/* ... */)
    {
        return ERROR;
    }

    if (/* ... */)
    {
        return WAIT;
    }

    // Try to allocate resources.
    /* ... */

    // Check safe state
    if (/* ... */)
    {
        // Restore previous allocation.
        /* ... */
        return WAIT_UNSAFE;
    }

    return SAFE;
}

//------------------------------------------------------------------------------
static bool_t state_safe(size_t n, size_t m)
{
    // Allocate work as a copy of available.
    uint32_t *work = memcpy(kmalloc(sizeof(uint32_t) * m), available,
                            sizeof(uint32_t) * m);

    // Allocate finish initialized with all false (zeros).
    uint32_t *finish = all(kmalloc(sizeof(uint32_t) * n), 0UL, n);
    uint32_t *all_true = all(kmalloc(sizeof(uint32_t) * n), 1UL, n);


    int i;
    // Loop while finish is not equal an array all true (ones).
    while (/* ... */)
    {
        // Find a task that can satisfy all the resources it needs.
        for (i = 0; i < n && (/* ... */ || /* ... */); i++);
        if (i == n)
        {
            // Free memory.
            kfree(work);
            kfree(finish);
            kfree(all_true);
            return false;
        }
        else
        {
            // Assume to make available the resources that the task found needs.
            /* ... */
        }
    }

    // Free memory.
    kfree(work);
    kfree(finish);
    kfree(all_true);
    return true;
}