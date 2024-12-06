/// @file resource_tracing.h
/// @brief Define the resources tracing system.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

/// @brief Initializes the resource registry and tracker.
void resource_register_init(void);

/// @brief Registers a new resource with a unique ID and name.
/// @param name The name of the resource to register.
/// @return The unique ID assigned to the resource, or -1 if registration failed.
int register_resource(const char *name);

/// @brief Retrieves the name of a registered resource.
/// @param id The unique ID of the resource.
/// @return The name of the resource, or NULL if not found.
const char *get_resource_name(int id);

/// @brief Unregisters a resource by its unique ID.
/// @param id The unique ID of the resource to unregister.
/// @return 0 on success, or -1 if the resource ID was not found.
int unregister_resource(int id);

/// @brief Prints the current registry of resources.
void print_resource_registry(void);

/// @brief Adds a resource to the tracking system.
/// @param resource_id The ID of the resource associated with the resource allocation.
/// @param file The file where the resource was allocated.
/// @param line The line number where the resource was allocated.
/// @param ptr The pointer or handle to the resource.
void store_resource_info(int resource_id, const char *file, int line, void *ptr);

/// @brief Removes a resource from the tracking system.
/// @param ptr The pointer or handle to the resource to remove.
void clear_resource_info(void *ptr);

/// @brief Checks and prints resource usage for a specific resource ID.
/// @details If resource_id is -1, checks and prints usage for all resources.
/// @param resource_id The ID of the resource to check. Use -1 to check all resources.
/// @param printer A callback function to handle the formatted output for each resource.
void print_resource_usage(int resource_id, const char* (*printer)(void *ptr));
