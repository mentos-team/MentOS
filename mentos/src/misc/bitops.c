///                MentOS, The Mentoring Operating system project
/// @file bitops.c
/// @brief Bitmasks functions.
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "bitops.h"
#include "video.h"

int find_first_bit(unsigned short int irq_mask)
{
	int i = 0;
	if (irq_mask == 0) {
		return 0;
	}
	for (i = 0; i < 8; i++) {
		if ((1 << i) & irq_mask) {
			break;
		}
	}

	return i;
}

bool_t has_flag(uint32_t flags, uint32_t flag)
{
	return (bool_t)((flags & flag) != 0);
}

void set_flag(uint32_t *flags, uint32_t flag)
{
	(*flags) |= flag;
}

void clear_flag(uint32_t *flags, uint32_t flag)
{
	(*flags) &= ~flag;
}
