/// @file test_switch.c
/// @brief Kernel context switch primitive tests.
/// @copyright (c) 2014-2024 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#include "tests/test.h"
#include "tests/test_utils.h"

extern int switch_to_round_trip(void);

TEST(switch_round_trip)
{
    TEST_SECTION_START("kernel context switch round trip");
    ASSERT(switch_to_round_trip() == 1);
    TEST_SECTION_END();
}

void test_switch(void)
{
    test_switch_round_trip();
}
