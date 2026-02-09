/**
 * @author Mirco De Marchi
 * @date   2/02/2021
 * @brief  Source file for deadlock deterministic simulation.
 * @copyright (c) University of Verona
 */

#include "deadlock_simulation.h"

#include "deadlock_prevention.h"
#include <arr_math.h>
#include <io/debug.h>
#include <mem/alloc/slab.h>
#include <string.h>
#include <sys/types.h>

#define SIM_N 2 ///< Task amount on simulation.
#define SIM_M 2 ///< Resource type amount on simulation.

/// @brief Simulation operations types.
typedef enum {
    FREE,
    LOCK,
} op_t;

/// @brief Structure type for a task resource request.
typedef struct request {
    pid_t req_task;          ///< Process id.
    op_t op;                 ///< Operation type.
    uint32_t req_vec[SIM_M]; ///< Resource request vector.
} req_t;

static void **kmalloc_matrix(size_t rows, size_t size)
{
    void **matrix = (void **)kmalloc(rows * sizeof(void *));
    if (!matrix) {
        return NULL;
    }

    for (size_t i = 0; i < rows; i++) {
        matrix[i] = kmalloc(size);
        if (!matrix[i]) {
            for (size_t j = 0; j < i; j++) {
                kfree(matrix[j]);
            }
            kfree(matrix);
            return NULL;
        }
    }

    return matrix;
}

static void kfree_matrix(void **matrix, size_t rows)
{
    if (!matrix) {
        return;
    }

    for (size_t i = 0; i < rows; i++) {
        kfree(matrix[i]);
    }
    kfree(matrix);
}

/// @brief Print available resources.
static void simulation_stats_available();

/// @brief Print resource request array.
/// @param req_vec Pointer to the resource request array.
/// @param length  Length of resource request array.
static void simulation_stats_request(uint32_t *req_vec, size_t length);

/// @brief Print stats of resources over tasks matrix.
/// @param header_str String name related with the matrix to print.
/// @param m          Matrix to print stats.
/// @param r          Row number of the matrix.
/// @param c          Col number of the matrix.
static void simulation_stats_matrix(const char *header_str, uint32_t **m, size_t r, size_t c);

/// @brief Print system stats.
static void simulation_stats();

/// @brief Simulate a semaphore try lock.
static deadlock_status_t simulation_try_lock(uint32_t *req_vec, size_t task_i, size_t n, size_t m);

/// @brief Simulate a semaphore lock.
static void simulation_lock(uint32_t *req_vec, pid_t pid);

/// @brief Simulate a semaphore free.
static void simulation_free(uint32_t *req_vec, pid_t pid);

/// @brief Simulation initialization.
static void simulation_init();

/// @brief Simulation core.
static void simulation_start();

/// @brief Simulation end.
static void simulation_close();

/// @brief Initial number of instances of resource type R_j currently available.
uint32_t initial_available[SIM_M]  = {1, 1};
/// @brief Initial matrix of maximum resource request that each task require.
uint32_t initial_max[SIM_N][SIM_M] = {
    {1, 1},
    {1, 1}
};
/// @brief Initial matrix of current resource allocation of each task.
uint32_t initial_alloc[SIM_N][SIM_M] = {
    {0, 0},
    {0, 0}
};

/// Array of resources instances currently available;
uint32_t *arr_available;
/// Matrix of the maximum resources instances that each task may require;
uint32_t **mat_max;
/// Matrix of current resources instances allocation of each task.
uint32_t **mat_alloc;
/// Matrix of current resources instances need of each task.
uint32_t **mat_need;

/// @brief Simulation requests.
req_t req_vec_test[] = {
    {.req_task = 0, .op = LOCK, .req_vec = {1, 0}},
    {.req_task = 1, .op = LOCK, .req_vec = {0, 1}},
    {.req_task = 0, .op = LOCK, .req_vec = {0, 1}},
    {.req_task = 1, .op = LOCK, .req_vec = {0, 1}},
    {.req_task = 0, .op = LOCK, .req_vec = {0, 1}},
    {.req_task = 0, .op = FREE, .req_vec = {0, 1}},
    {.req_task = 1, .op = LOCK, .req_vec = {0, 1}},
    {.req_task = 0, .op = FREE, .req_vec = {1, 0}},
    {.req_task = 1, .op = LOCK, .req_vec = {1, 0}},
    {.req_task = 1, .op = FREE, .req_vec = {1, 0}},
    {.req_task = 1, .op = FREE, .req_vec = {0, 1}},
    {.req_task = 1, .op = FREE, .req_vec = {0, 1}},
};

static void simulation_stats_available()
{
    pr_debug(" { ");
    for (size_t j = 0; j < SIM_M - 1; j++)
        pr_debug("R_%i: %u, ", j, arr_available[j]);
    pr_debug("R_%d: %u }", SIM_M - 1, arr_available[SIM_M - 1]);
}

static void simulation_stats_request(uint32_t *req_vec, size_t length)
{
    pr_debug(" { ");
    for (size_t j = 0; j < length - 1; j++)
        pr_debug("R_%i: %u, ", j, req_vec[j]);
    pr_debug("R_%i: %u }", length - 1, req_vec[length - 1]);
}

static void simulation_stats_matrix(const char *header_str, uint32_t **m, size_t r, size_t c)
{
    const int col_size   = 7;
    const size_t tot_col = c + 1;
    size_t j             = 0;

    pr_debug(" ");
    for (j = 0; j < (tot_col * col_size) - 1; j++)
        pr_debug("-");
    pr_debug("\n");

    pr_debug("|");
    const size_t table_space = (tot_col * col_size) - strlen(header_str) - 1;
    for (j = 0; j < table_space / 2; j++)
        pr_debug(" ");
    pr_debug("%s", header_str);
    for (; j < table_space; j++)
        pr_debug(" ");
    pr_debug("|\n");

    pr_debug("| Task |");
    for (j = 0; j < c; j++)
        pr_debug(" R_%-2i |", j);
    pr_debug("\n");

    for (size_t i = 0; i < r; i++) {
        pr_debug("| %4i |", i);
        for (j = 0; j < c; j++)
            pr_debug(" %4u |", m[i][j]);
        pr_debug("\n");
    }

    pr_debug(" ");
    for (j = 0; j < (tot_col * col_size) - 1; j++)
        pr_debug("-");
    pr_debug("\n");
}

static void simulation_stats()
{
    pr_debug("Tasks N:      %i\n", SIM_N);
    pr_debug("Resources M:  %i\n", SIM_M);

    pr_debug("AVAILABLE:");
    simulation_stats_available();
    pr_debug("\n");

    simulation_stats_matrix("MAX", mat_max, SIM_N, SIM_M);
    simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
    simulation_stats_matrix("NEED", mat_need, SIM_N, SIM_M);
}

static deadlock_status_t simulation_try_lock(uint32_t *req_vec, size_t task_i, size_t n, size_t m)
{
#if ENABLE_DEADLOCK_PREVENTION
    return request(req_vec, task_i, arr_available, mat_alloc, mat_need, n, m);
#else
    return ERROR;
#endif
}

static void simulation_lock(uint32_t *req_vec, pid_t pid)
{
    if (!(arr_available && mat_max && mat_alloc && mat_need)) {
        pr_debug("Some task-resource matrices NULL\n");
        return;
    }

    switch (simulation_try_lock(req_vec, pid, SIM_N, SIM_M)) {
    case SAFE:
        pr_debug("LOCK (task: %d; req_vec:", pid);
        simulation_stats_request(req_vec, SIM_M);
        pr_debug(") SAFE: enjoy your resource\n");
        pr_debug("available:");
        simulation_stats_available();
        pr_debug("\n");
        simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
        break;
    case WAIT:
        pr_debug("LOCK (task %d; req_vec:", pid);
        simulation_stats_request(req_vec, SIM_M);
        pr_debug(") WAIT: resource busy\n");
        pr_debug("available:");
        simulation_stats_available();
        pr_debug("\n");
        simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
        break;
    case WAIT_UNSAFE:
        pr_debug("LOCK (task %d; rec_vec:", pid);
        simulation_stats_request(req_vec, SIM_M);
        pr_debug(") WAIT UNSAFE: deadlock detected\n");
        pr_debug("available:");
        simulation_stats_available();
        pr_debug("\n");
        simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
        break;
    case ERROR:
        pr_debug("LOCK (task %d; rec_vec:", pid);
        simulation_stats_request(req_vec, SIM_M);
        pr_debug(") ERROR: max matrix overflow\n");
        pr_debug("available:");
        simulation_stats_available();
        pr_debug("\n");
        simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
        break;
    default:
        return;
    }
}

static void simulation_free(uint32_t *req_vec, pid_t pid)
{
    if (arr_l_any(mat_alloc[pid], req_vec, SIM_M)) {
        pr_debug("FREE (task %d; rec_vec:", pid);
        simulation_stats_request(req_vec, SIM_M);
        pr_debug(") ERROR: try to free a resource not own\n");
        pr_debug("available:");
        simulation_stats_available();
        pr_debug("\n");
        simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
        return;
    }

    arr_add(arr_available, req_vec, SIM_M);
    arr_sub(mat_alloc[pid], req_vec, SIM_M);
    // Check what happen if you uncomment the following line.
    // arr_add(mat_need[pid], req_vec, SIM_M);

    pr_debug("FREE (task %d; rec_vec:", pid);
    simulation_stats_request(req_vec, SIM_M);
    pr_debug(")\n");
    pr_debug("available:");
    simulation_stats_available();
    pr_debug("\n");
    simulation_stats_matrix("ALLOC", mat_alloc, SIM_N, SIM_M);
}

static void simulation_init()
{
    arr_available = (uint32_t *)kmalloc(SIM_M * sizeof(uint32_t));
    mat_max       = (uint32_t **)kmalloc_matrix(SIM_N, SIM_M * sizeof(uint32_t));
    mat_alloc     = (uint32_t **)kmalloc_matrix(SIM_N, SIM_M * sizeof(uint32_t));
    mat_need      = (uint32_t **)kmalloc_matrix(SIM_N, SIM_M * sizeof(uint32_t));

    if (!(arr_available && mat_max && mat_alloc && mat_need)) {
        pr_debug("Deadlock Prevention: simulation allocation failed\n");
        return;
    }

    memcpy(arr_available, initial_available, SIM_M * sizeof(uint32_t));
    for (size_t i = 0; i < SIM_N; i++) {
        memcpy(mat_max[i], initial_max[i], SIM_M * sizeof(uint32_t));
        memcpy(mat_alloc[i], initial_alloc[i], SIM_M * sizeof(uint32_t));
    }

    // Calculate mat_need[i][j] = mat_max[i][j] - mat_alloc[i][j].
    for (size_t i = 0; i < SIM_N; i++) {
        memcpy(mat_need[i], mat_max[i], SIM_M * sizeof(uint32_t));
        arr_sub(mat_need[i], mat_alloc[i], SIM_M);
    }
}

static void simulation_start()
{
    pr_debug("Deadlock Prevention: simulation start\n");
    for (size_t test = 0; test < sizeof(req_vec_test) / sizeof(req_t); test++) {
        uint32_t *req_vec = req_vec_test[test].req_vec;
        pid_t task_pid    = req_vec_test[test].req_task;

        switch (req_vec_test[test].op) {
        case FREE: {
            simulation_free(req_vec, task_pid);
            break;
        }
        case LOCK: {
            simulation_lock(req_vec, task_pid);
            break;
        }
        default: {
            pr_debug("Request vector operation type not recognized\n");
            break;
        }
        }
    }
}

static void simulation_close()
{
    if (arr_available) {
        kfree(arr_available);
    }
    if (mat_max) {
        kfree_matrix((void **)mat_max, SIM_N);
    }
    if (mat_alloc) {
        kfree_matrix((void **)mat_alloc, SIM_N);
    }
    if (mat_need) {
        kfree_matrix((void **)mat_need, SIM_N);
    }
    mat_max   = NULL;
    mat_alloc = NULL;
    mat_need  = NULL;
}

void deadlock_simulation(void)
{
#if ENABLE_DEADLOCK_PREVENTION
    pr_debug("Deadlock Prevention: enabled\n");
    simulation_init();
    if (!(arr_available && mat_max && mat_alloc && mat_need)) {
        simulation_close();
        return;
    }
    simulation_stats();
    simulation_start();
    simulation_close();
#else
    pr_debug("Deadlock Prevention: disabled\n");
#endif
}