/**
 * @author Mirco De Marchi
 * @date   2/02/2021
 * @brief  Header file for deadlock prevention algorithms.
 * @copyright (c) University of Verona
 */

#pragma once

#include "stdio.h"
#include "stdlib.h"
#include "stdio.h"
#include "stdbool.h"
#include "string.h"

/// @brief Resource allocation request status enumeration.
typedef enum {
    SAFE,        ///< State safe.
    WAIT,        ///< State waiting.
    WAIT_UNSAFE, ///< State waiting for unsafe detection.
    ERROR,       ///< State error.
} deadlock_status_t;

/// @brief Request of resources perfomed by a task.
/// @param req_vec Array pointer of resource request for each task in the
///                system.
/// @param task_i  Index of task that perform the request to use as array index.
/// @param arr_available    Array of resources instances currently available.
/// @param mat_alloc        Matrix of current resources instances allocation of
///                         each task.
/// @param mat_need         Matrix of current resources instances need of each
///                         task. need[i][j] = max[i][j] - alloc[i][j]
/// @param n       Number of tasks currently in the system.
/// @param m       Number of resource types in the system (length of req_vec).
/// @return Status of the request (see status_t enum).
deadlock_status_t request(uint32_t *req_vec, size_t task_i,
        uint32_t *arr_available, uint32_t ** mat_alloc, uint32_t **mat_need,
        size_t n, size_t m);
