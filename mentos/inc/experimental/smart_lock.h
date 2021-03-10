///                MentOS, The Mentoring Operating system project
/// @file smart_lock.h
/// @brief
/// @copyright (c) 2019 This file is distributed under the MIT License.
/// See LICENSE.md for details.

#pragma once

#include "stdatomic.h"

int lock_create();

int lock_destroy(int id);

int lock_init(int id);

int lock_try_acquire(int id);

int lock_release(int id);
