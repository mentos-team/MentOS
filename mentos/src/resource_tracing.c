/// @file resource_tracing.c
/// @brief Implement the resources tracing.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "resource_tracing.h"

// Setup the logging for this file (do this before any other include).
#include "sys/kernel_levels.h"           // Include kernel log levels.
#define __DEBUG_HEADER__ "[RESREG]"      ///< Change header.
#define __DEBUG_LEVEL__  LOGLEVEL_NOTICE ///< Set log level.
#include "io/debug.h"                    // Include debugging functions.

#define MAX_TRACKED_RESOURCES    1024
#define MAX_REGISTERED_RESOURCES 128

static struct {
    int id;
    const char *name;
} resource_registry[MAX_REGISTERED_RESOURCES];

static struct {
    int resource_id;
    const char *file;
    int line;
    void *ptr;
} resource_tracker[MAX_TRACKED_RESOURCES];

void resource_register_init(void)
{
    for (unsigned i = 0; i < MAX_REGISTERED_RESOURCES; ++i) {
        resource_registry[i].id   = -1;
        resource_registry[i].name = 0;
    }
    for (unsigned i = 0; i < MAX_TRACKED_RESOURCES; ++i) {
        resource_tracker[i].resource_id = 0;
        resource_tracker[i].file        = 0;
        resource_tracker[i].line        = 0;
        resource_tracker[i].ptr         = 0;
    }
}

int register_resource(const char *name)
{
    for (unsigned i = 0; i < MAX_REGISTERED_RESOURCES; ++i) {
        if (resource_registry[i].id == -1) {
            resource_registry[i].id   = i;
            resource_registry[i].name = name;
            return i;
        }
    }
    return -1;
}

const char *get_resource_name(int id)
{
    if (id >= 0) {
        for (unsigned i = 0; i < MAX_REGISTERED_RESOURCES; ++i) {
            if (resource_registry[i].id == id) {
                return resource_registry[i].name;
            }
        }
    }
    return 0;
}

int unregister_resource(int id)
{
    if (id >= 0) {
        for (int i = 0; i < MAX_REGISTERED_RESOURCES; ++i) {
            if (resource_registry[i].id == id) {
                resource_registry[i].id   = -1;
                resource_registry[i].name = 0;
                return 0;
            }
        }
    }
    return -1;
}

void print_resource_registry(void)
{
    pr_notice("Resource Registry:\n");
    for (int i = 0; i < MAX_REGISTERED_RESOURCES; ++i) {
        if (resource_registry[i].id >= 0) {
            pr_notice("    ID=%2d, Name=%s\n", resource_registry[i].id, resource_registry[i].name);
        }
    }
}

void store_resource_info(int resource_id, const char *file, int line, void *ptr)
{
    for (unsigned i = 0; i < MAX_TRACKED_RESOURCES; ++i) {
        if (resource_tracker[i].ptr == 0) {
            resource_tracker[i].resource_id = resource_id;
            resource_tracker[i].file        = file;
            resource_tracker[i].line        = line;
            resource_tracker[i].ptr         = ptr;
            return;
        }
    }
}

void clear_resource_info(void *ptr)
{
    for (unsigned i = 0; i < MAX_TRACKED_RESOURCES; ++i) {
        if (resource_tracker[i].ptr == ptr) {
            resource_tracker[i].resource_id = -1;
            resource_tracker[i].file        = 0;
            resource_tracker[i].line        = -1;
            resource_tracker[i].ptr         = 0;
            return;
        }
    }
}

void print_resource_usage(int resource_id, const char *(*printer)(void *ptr))
{
    pr_notice("Checking resource usage (resource_id=%d, name: %s):\n", resource_id, get_resource_name(resource_id));
    for (unsigned i = 0; i < MAX_TRACKED_RESOURCES; ++i) {
        if (resource_tracker[i].ptr && (resource_id == -1 || (resource_tracker[i].resource_id == resource_id))) {
            if (printer) {
                pr_notice("    %s:%d, %s\n",
                          resource_tracker[i].file,
                          resource_tracker[i].line,
                          printer(resource_tracker[i].ptr));
            } else {
                pr_notice("    ptr=0x%p, consumed at %s:%d\n",
                          resource_tracker[i].ptr,
                          resource_tracker[i].file,
                          resource_tracker[i].line);
            }
        }
    }
}
