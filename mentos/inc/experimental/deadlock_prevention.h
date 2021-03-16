/**
 * @author Mirco De Marchi
 * @date   2/02/2021
 * @brief  Header file for deadlock prevention algorithms.
 * @copyright (c) University of Verona
 */

#ifndef DEADLOCK_PREVENTION_H_
#define DEADLOCK_PREVENTION_H_

#include "stdio.h"
#include "stdlib.h"
#include "stdio.h"
#include "stdbool.h"
#include "string.h"
//------------------------------------------------------------------------------
/// @brief Resource allocation request status enumeration.
typedef enum {
    SAFE,        ///< State safe.
    WAIT,        ///< State waiting.
    WAIT_UNSAFE, ///< State waiting for unsafe detection.
    ERROR,       ///< State error.
} status_t;
//------------------------------------------------------------------------------

/// @brief Number of instances of each resources currently available.
extern uint32_t *  available;
/// @brief Matrix of maximum resources instances that each task may need. 
extern uint32_t ** max;
/// @brief Matrix of current resource allocation for each task.
extern uint32_t ** alloc;
/** 
 * @brief Matrix of current resource needs for each task.
 * need[i][j] = max[i][j] - alloc[i][j]
 */
extern uint32_t ** need;
//------------------------------------------------------------------------------

/**
 * @brief Request of resources perfomed by a task.
 * @param req_vec Array pointer of resource request for each task in the system.
 * @param task_i  Index of task that perform the request to use as array index.
 * @param n       Number of tasks currently in the system (length of req_vec).
 * @param m       Number of resource types in the system.
 * @return Status of the request (see status_t enum).
 */
status_t request(uint32_t *req_vec, size_t task_i, size_t n, size_t m);

#endif  // DEADLOCK_PREVENTION_H_