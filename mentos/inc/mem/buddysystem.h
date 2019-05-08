///                MentOS, The Mentoring Operating system project
/// @file buddysystem.h
/// @brief Buddy System.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "zone_allocator.h"

/// @brief        Allocate a block of page frames in a zone of size 2^{order.
/// @param  zone  A memory zone.
/// @param  order The logarithm of the size of the block.
/// @return       The address of the first page descriptor of the block, or NULL.
page_t *bb_alloc_pages(zone_t *zone, unsigned int order);

/// @brief       Free a block of page frames in a zone of size 2^{order.
/// @param zone  A memory zone.
/// @param page  The address of the first page descriptor of the block.
/// @param order The logarithm of the size of the block.
void bb_free_pages(zone_t *zone, page_t *page, unsigned int order);

/// @brief      Print the size of free_list of each free_area.
/// @param zone A memory zone.
void buddy_system_dump(zone_t *zone);
